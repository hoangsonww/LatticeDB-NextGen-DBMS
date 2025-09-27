#!/bin/bash

set -e

# AWS ECS Service Management Script

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TERRAFORM_DIR="$(dirname "$SCRIPT_DIR")"
AWS_REGION="${AWS_REGION:-us-west-2}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} ✅ $1"
}

log_warning() {
    echo -e "${YELLOW}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} ⚠️  $1"
}

log_error() {
    echo -e "${RED}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} ❌ $1"
}

# Get Terraform outputs
get_cluster_name() {
    cd "$TERRAFORM_DIR"
    terraform output -raw ecs_cluster_name 2>/dev/null || echo ""
}

get_service_name() {
    cd "$TERRAFORM_DIR"
    terraform output -raw ecs_service_name 2>/dev/null || echo ""
}

get_load_balancer_dns() {
    cd "$TERRAFORM_DIR"
    terraform output -raw load_balancer_dns_name 2>/dev/null || echo ""
}

# Check if Terraform state exists
check_terraform_state() {
    if [ ! -f "$TERRAFORM_DIR/terraform.tfstate" ]; then
        log_error "Terraform state not found. Please run 'terraform apply' first."
        exit 1
    fi
}

# Show service status
show_status() {
    log "Getting service status..."

    local cluster_name=$(get_cluster_name)
    local service_name=$(get_service_name)
    local lb_dns=$(get_load_balancer_dns)

    if [ -z "$cluster_name" ] || [ -z "$service_name" ]; then
        log_error "Could not get cluster or service name from Terraform state"
        exit 1
    fi

    echo ""
    echo "🔍 Service Status"
    echo "=================="
    echo "Cluster: $cluster_name"
    echo "Service: $service_name"
    echo "Region:  $AWS_REGION"
    echo ""

    # Service details
    aws ecs describe-services \
        --cluster "$cluster_name" \
        --services "$service_name" \
        --region "$AWS_REGION" \
        --output table \
        --query 'services[0].{Status:status,RunningCount:runningCount,DesiredCount:desiredCount,PendingCount:pendingCount}'

    # Task details
    echo ""
    echo "📋 Tasks"
    echo "========"
    aws ecs list-tasks \
        --cluster "$cluster_name" \
        --service-name "$service_name" \
        --region "$AWS_REGION" \
        --output table \
        --query 'taskArns[*]' | head -10

    # Health check
    if [ -n "$lb_dns" ]; then
        echo ""
        echo "🏥 Health Check"
        echo "==============="
        local health_url="http://$lb_dns/health"
        echo "URL: $health_url"

        if curl -f -s --max-time 10 "$health_url" > /dev/null; then
            log_success "Health check passed"
        else
            log_warning "Health check failed"
        fi
    fi
}

# Scale service
scale_service() {
    local desired_count="$1"

    if [ -z "$desired_count" ] || ! [[ "$desired_count" =~ ^[0-9]+$ ]]; then
        log_error "Invalid desired count: $desired_count"
        echo "Usage: $0 scale <count>"
        exit 1
    fi

    local cluster_name=$(get_cluster_name)
    local service_name=$(get_service_name)

    log "Scaling service to $desired_count tasks..."

    aws ecs update-service \
        --cluster "$cluster_name" \
        --service "$service_name" \
        --desired-count "$desired_count" \
        --region "$AWS_REGION" \
        --output table \
        --query 'service.{Status:status,RunningCount:runningCount,DesiredCount:desiredCount,PendingCount:pendingCount}'

    log_success "Scale operation initiated"

    # Wait for stable state
    log "Waiting for service to stabilize..."
    aws ecs wait services-stable \
        --cluster "$cluster_name" \
        --services "$service_name" \
        --region "$AWS_REGION"

    log_success "Service scaled successfully"
}

# Restart service
restart_service() {
    local cluster_name=$(get_cluster_name)
    local service_name=$(get_service_name)

    log "Restarting service (force new deployment)..."

    aws ecs update-service \
        --cluster "$cluster_name" \
        --service "$service_name" \
        --force-new-deployment \
        --region "$AWS_REGION" \
        --output table \
        --query 'service.{Status:status,TaskDefinition:taskDefinition}'

    log_success "Restart initiated"

    # Wait for stable state
    log "Waiting for service to stabilize..."
    aws ecs wait services-stable \
        --cluster "$cluster_name" \
        --services "$service_name" \
        --region "$AWS_REGION"

    log_success "Service restarted successfully"
}

# Update service with new image
update_service() {
    local image_tag="$1"

    if [ -z "$image_tag" ]; then
        log_error "Image tag is required"
        echo "Usage: $0 update <image-tag>"
        exit 1
    fi

    local cluster_name=$(get_cluster_name)
    local service_name=$(get_service_name)

    log "Updating service with image tag: $image_tag"

    # Get current task definition
    local current_task_def=$(aws ecs describe-services \
        --cluster "$cluster_name" \
        --services "$service_name" \
        --region "$AWS_REGION" \
        --query 'services[0].taskDefinition' \
        --output text)

    log "Current task definition: $current_task_def"

    # Get task definition details
    local task_def_json=$(aws ecs describe-task-definition \
        --task-definition "$current_task_def" \
        --region "$AWS_REGION")

    # Get ECR repository URL
    cd "$TERRAFORM_DIR"
    local ecr_repo=$(terraform output -raw ecr_repository_url)

    # Update image in task definition
    local new_task_def=$(echo "$task_def_json" | jq --arg image "${ecr_repo}:${image_tag}" '
        .taskDefinition |
        del(.taskDefinitionArn, .revision, .status, .requiresAttributes, .placementConstraints, .compatibilities, .registeredAt, .registeredBy) |
        .containerDefinitions[0].image = $image
    ')

    # Register new task definition
    log "Registering new task definition..."
    local new_task_def_arn=$(echo "$new_task_def" | aws ecs register-task-definition \
        --region "$AWS_REGION" \
        --cli-input-json file:///dev/stdin \
        --query 'taskDefinition.taskDefinitionArn' \
        --output text)

    log "New task definition: $new_task_def_arn"

    # Update service
    log "Updating service..."
    aws ecs update-service \
        --cluster "$cluster_name" \
        --service "$service_name" \
        --task-definition "$new_task_def_arn" \
        --region "$AWS_REGION" \
        --output table \
        --query 'service.{Status:status,TaskDefinition:taskDefinition}'

    log_success "Service update initiated"

    # Wait for stable state
    log "Waiting for service to stabilize..."
    aws ecs wait services-stable \
        --cluster "$cluster_name" \
        --services "$service_name" \
        --region "$AWS_REGION"

    log_success "Service updated successfully"
}

# Show service logs
show_logs() {
    local follow="${1:-false}"
    local cluster_name=$(get_cluster_name)

    cd "$TERRAFORM_DIR"
    local log_group=$(terraform output -raw cloudwatch_log_group_name)

    if [ -z "$log_group" ]; then
        log_error "Could not get log group name from Terraform state"
        exit 1
    fi

    log "Showing logs from: $log_group"
    echo ""

    if [ "$follow" = "true" ]; then
        aws logs tail "$log_group" --follow --region "$AWS_REGION"
    else
        aws logs tail "$log_group" --since 1h --region "$AWS_REGION"
    fi
}

# Execute command in running task
exec_command() {
    local command="$1"

    if [ -z "$command" ]; then
        command="/bin/bash"
    fi

    local cluster_name=$(get_cluster_name)
    local service_name=$(get_service_name)

    # Get running task ARN
    local task_arn=$(aws ecs list-tasks \
        --cluster "$cluster_name" \
        --service-name "$service_name" \
        --region "$AWS_REGION" \
        --desired-status RUNNING \
        --query 'taskArns[0]' \
        --output text)

    if [ "$task_arn" = "None" ] || [ -z "$task_arn" ]; then
        log_error "No running tasks found"
        exit 1
    fi

    log "Executing command in task: $task_arn"

    aws ecs execute-command \
        --cluster "$cluster_name" \
        --task "$task_arn" \
        --container "latticedb" \
        --interactive \
        --command "$command" \
        --region "$AWS_REGION"
}

# Show service events
show_events() {
    local cluster_name=$(get_cluster_name)
    local service_name=$(get_service_name)

    log "Recent service events..."
    echo ""

    aws ecs describe-services \
        --cluster "$cluster_name" \
        --services "$service_name" \
        --region "$AWS_REGION" \
        --query 'services[0].events[0:10]' \
        --output table
}

# Show service metrics
show_metrics() {
    local cluster_name=$(get_cluster_name)
    local service_name=$(get_service_name)

    log "Service metrics (last 1 hour)..."
    echo ""

    local end_time=$(date -u +%Y-%m-%dT%H:%M:%S)
    local start_time=$(date -u -d '1 hour ago' +%Y-%m-%dT%H:%M:%S)

    # CPU utilization
    echo "📊 CPU Utilization:"
    aws cloudwatch get-metric-statistics \
        --namespace AWS/ECS \
        --metric-name CPUUtilization \
        --dimensions Name=ServiceName,Value="$service_name" Name=ClusterName,Value="$cluster_name" \
        --start-time "$start_time" \
        --end-time "$end_time" \
        --period 300 \
        --statistics Average \
        --region "$AWS_REGION" \
        --query 'Datapoints[*].{Time:Timestamp,Value:Average}' \
        --output table

    echo ""
    echo "💾 Memory Utilization:"
    aws cloudwatch get-metric-statistics \
        --namespace AWS/ECS \
        --metric-name MemoryUtilization \
        --dimensions Name=ServiceName,Value="$service_name" Name=ClusterName,Value="$cluster_name" \
        --start-time "$start_time" \
        --end-time "$end_time" \
        --period 300 \
        --statistics Average \
        --region "$AWS_REGION" \
        --query 'Datapoints[*].{Time:Timestamp,Value:Average}' \
        --output table
}

# Stop all tasks (maintenance mode)
stop_service() {
    local cluster_name=$(get_cluster_name)
    local service_name=$(get_service_name)

    log_warning "Stopping service (scaling to 0)..."

    aws ecs update-service \
        --cluster "$cluster_name" \
        --service "$service_name" \
        --desired-count 0 \
        --region "$AWS_REGION" \
        --output table \
        --query 'service.{Status:status,RunningCount:runningCount,DesiredCount:desiredCount}'

    log_success "Service stopped"
}

# Usage information
usage() {
    echo "Usage: $0 <command> [arguments]"
    echo ""
    echo "Commands:"
    echo "  status              Show service status"
    echo "  scale <count>       Scale service to specified count"
    echo "  restart             Restart service (force new deployment)"
    echo "  update <tag>        Update service with new image tag"
    echo "  logs [--follow]     Show service logs"
    echo "  exec [command]      Execute command in running task"
    echo "  events              Show recent service events"
    echo "  metrics             Show service metrics"
    echo "  stop                Stop service (scale to 0)"
    echo ""
    echo "Examples:"
    echo "  $0 status           # Show current status"
    echo "  $0 scale 3          # Scale to 3 instances"
    echo "  $0 update v1.2.0    # Update to version 1.2.0"
    echo "  $0 logs --follow    # Follow logs in real-time"
    echo "  $0 exec 'ps aux'    # Execute command in container"
}

# Main function
main() {
    if [ $# -eq 0 ]; then
        usage
        exit 1
    fi

    local command="$1"
    shift

    # Check dependencies
    if ! command -v aws &> /dev/null; then
        log_error "AWS CLI is not installed"
        exit 1
    fi

    if ! command -v jq &> /dev/null; then
        log_error "jq is not installed"
        exit 1
    fi

    check_terraform_state

    case "$command" in
        status)
            show_status
            ;;
        scale)
            scale_service "$1"
            ;;
        restart)
            restart_service
            ;;
        update)
            update_service "$1"
            ;;
        logs)
            local follow="false"
            if [ "$1" = "--follow" ]; then
                follow="true"
            fi
            show_logs "$follow"
            ;;
        exec)
            exec_command "$*"
            ;;
        events)
            show_events
            ;;
        metrics)
            show_metrics
            ;;
        stop)
            stop_service
            ;;
        help|--help|-h)
            usage
            ;;
        *)
            log_error "Unknown command: $command"
            usage
            exit 1
            ;;
    esac
}

# Run main function
main "$@"