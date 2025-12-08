#!/bin/bash

# Blue/Green Deployment Script for AWS ECS
# This script implements a safe blue/green deployment strategy with automated rollback

set -euo pipefail

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
ENVIRONMENT=${ENVIRONMENT:-"staging"}
AWS_REGION=${AWS_REGION:-"us-east-1"}
CLUSTER_NAME="latticedb-${ENVIRONMENT}"
SERVICE_NAME="latticedb-service-${ENVIRONMENT}"
TASK_FAMILY="latticedb-task-${ENVIRONMENT}"
HEALTH_CHECK_PATH=${HEALTH_CHECK_PATH:-"/health"}
HEALTH_CHECK_RETRIES=${HEALTH_CHECK_RETRIES:-30}
HEALTH_CHECK_INTERVAL=${HEALTH_CHECK_INTERVAL:-10}
ROLLBACK_ON_FAILURE=${ROLLBACK_ON_FAILURE:-true}
APPROVAL_REQUIRED=${APPROVAL_REQUIRED:-true}

# Usage function
usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Blue/Green Deployment Script for AWS ECS

OPTIONS:
    -e, --environment ENV       Environment (dev/staging/prod) [default: staging]
    -r, --region REGION         AWS region [default: us-east-1]
    -c, --cluster CLUSTER       ECS cluster name [default: latticedb-ENV]
    -s, --service SERVICE       ECS service name [default: latticedb-service-ENV]
    -i, --image IMAGE           Docker image URI (required)
    -t, --task-family FAMILY    Task definition family [default: latticedb-task-ENV]
    --no-approval               Skip manual approval before traffic switch
    --no-rollback              Disable automatic rollback on failure
    -h, --help                  Show this help message

EXAMPLES:
    # Deploy to staging with manual approval
    $0 -e staging -i 123456789.dkr.ecr.us-east-1.amazonaws.com/latticedb:v1.2.3

    # Deploy to production with automatic approval
    $0 -e prod -i 123456789.dkr.ecr.us-east-1.amazonaws.com/latticedb:v1.2.3 --no-approval

    # Deploy with custom cluster name
    $0 -e staging -c my-cluster -s my-service -i image:tag

EOF
    exit 1
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -e|--environment)
            ENVIRONMENT="$2"
            shift 2
            ;;
        -r|--region)
            AWS_REGION="$2"
            shift 2
            ;;
        -c|--cluster)
            CLUSTER_NAME="$2"
            shift 2
            ;;
        -s|--service)
            SERVICE_NAME="$2"
            shift 2
            ;;
        -i|--image)
            IMAGE_URI="$2"
            shift 2
            ;;
        -t|--task-family)
            TASK_FAMILY="$2"
            shift 2
            ;;
        --no-approval)
            APPROVAL_REQUIRED=false
            shift
            ;;
        --no-rollback)
            ROLLBACK_ON_FAILURE=false
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            ;;
    esac
done

# Validate required parameters
if [[ -z "${IMAGE_URI:-}" ]]; then
    echo -e "${RED}Error: Docker image URI is required (-i/--image)${NC}"
    usage
fi

# Validate environment
if [[ ! "$ENVIRONMENT" =~ ^(dev|staging|prod)$ ]]; then
    echo -e "${RED}Error: Environment must be dev, staging, or prod${NC}"
    exit 1
fi

# Log function
log() {
    echo -e "${BLUE}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"
}

log_error() {
    echo -e "${RED}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"
}

# Get current task definition
get_current_task_definition() {
    log "Fetching current task definition..."
    aws ecs describe-task-definition \
        --region "$AWS_REGION" \
        --task-definition "$TASK_FAMILY" \
        --query 'taskDefinition' \
        --output json > /tmp/current-task-def.json

    CURRENT_TASK_DEF_ARN=$(jq -r '.taskDefinitionArn' /tmp/current-task-def.json)
    log_success "Current task definition: $CURRENT_TASK_DEF_ARN"
}

# Create new task definition with new image
create_new_task_definition() {
    log "Creating new task definition with image: $IMAGE_URI"

    # Extract the current task definition and modify the image
    jq --arg IMAGE "$IMAGE_URI" \
        'del(.taskDefinitionArn, .revision, .status, .requiresAttributes, .compatibilities, .registeredAt, .registeredBy) |
         .containerDefinitions[0].image = $IMAGE' \
        /tmp/current-task-def.json > /tmp/new-task-def.json

    # Register the new task definition
    NEW_TASK_DEF_ARN=$(aws ecs register-task-definition \
        --region "$AWS_REGION" \
        --cli-input-json file:///tmp/new-task-def.json \
        --query 'taskDefinition.taskDefinitionArN' \
        --output text)

    log_success "New task definition created: $NEW_TASK_DEF_ARN"
}

# Get target group ARNs
get_target_groups() {
    log "Fetching target group ARNs for service..."

    # Get the load balancer configuration from the service
    aws ecs describe-services \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --services "$SERVICE_NAME" \
        --query 'services[0].loadBalancers' \
        --output json > /tmp/load-balancers.json

    BLUE_TG_ARN=$(jq -r '.[0].targetGroupArn // empty' /tmp/load-balancers.json)

    if [[ -z "$BLUE_TG_ARN" ]]; then
        log_error "No target group found for service. Blue/green requires ALB with target groups."
        exit 1
    fi

    log_success "Blue target group: $BLUE_TG_ARN"
}

# Create green target group
create_green_target_group() {
    log "Creating green target group for new version..."

    # Get VPC and details from blue target group
    BLUE_TG_CONFIG=$(aws elbv2 describe-target-groups \
        --region "$AWS_REGION" \
        --target-group-arns "$BLUE_TG_ARN" \
        --query 'TargetGroups[0]' \
        --output json)

    VPC_ID=$(echo "$BLUE_TG_CONFIG" | jq -r '.VpcId')
    PROTOCOL=$(echo "$BLUE_TG_CONFIG" | jq -r '.Protocol')
    PORT=$(echo "$BLUE_TG_CONFIG" | jq -r '.Port')
    HC_PROTOCOL=$(echo "$BLUE_TG_CONFIG" | jq -r '.HealthCheckProtocol')
    HC_PORT=$(echo "$BLUE_TG_CONFIG" | jq -r '.HealthCheckPort')
    HC_PATH=$(echo "$BLUE_TG_CONFIG" | jq -r '.HealthCheckPath')
    HC_INTERVAL=$(echo "$BLUE_TG_CONFIG" | jq -r '.HealthCheckIntervalSeconds')
    HC_TIMEOUT=$(echo "$BLUE_TG_CONFIG" | jq -r '.HealthCheckTimeoutSeconds')
    HC_HEALTHY=$(echo "$BLUE_TG_CONFIG" | jq -r '.HealthyThresholdCount')
    HC_UNHEALTHY=$(echo "$BLUE_TG_CONFIG" | jq -r '.UnhealthyThresholdCount')

    # Create green target group with same configuration
    GREEN_TG_NAME="latticedb-green-${ENVIRONMENT}-$(date +%s)"

    GREEN_TG_ARN=$(aws elbv2 create-target-group \
        --region "$AWS_REGION" \
        --name "$GREEN_TG_NAME" \
        --protocol "$PROTOCOL" \
        --port "$PORT" \
        --vpc-id "$VPC_ID" \
        --health-check-protocol "$HC_PROTOCOL" \
        --health-check-port "$HC_PORT" \
        --health-check-path "$HC_PATH" \
        --health-check-interval-seconds "$HC_INTERVAL" \
        --health-check-timeout-seconds "$HC_TIMEOUT" \
        --healthy-threshold-count "$HC_HEALTHY" \
        --unhealthy-threshold-count "$HC_UNHEALTHY" \
        --target-type ip \
        --tags "Key=Environment,Value=$ENVIRONMENT" "Key=DeploymentType,Value=green" \
        --query 'TargetGroups[0].TargetGroupArn' \
        --output text)

    log_success "Green target group created: $GREEN_TG_ARN"
}

# Deploy green version
deploy_green_version() {
    log "Deploying green version with new task definition..."

    # Get current service configuration
    aws ecs describe-services \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --services "$SERVICE_NAME" \
        --query 'services[0]' \
        --output json > /tmp/current-service.json

    DESIRED_COUNT=$(jq -r '.desiredCount' /tmp/current-service.json)

    # Create a temporary green service
    GREEN_SERVICE_NAME="${SERVICE_NAME}-green"

    log "Creating green service with $DESIRED_COUNT tasks..."

    # Extract necessary service configuration
    NETWORK_CONFIG=$(jq -r '.networkConfiguration' /tmp/current-service.json)

    aws ecs create-service \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --service-name "$GREEN_SERVICE_NAME" \
        --task-definition "$NEW_TASK_DEF_ARN" \
        --desired-count "$DESIRED_COUNT" \
        --launch-type FARGATE \
        --network-configuration "$NETWORK_CONFIG" \
        --load-balancers "targetGroupArn=$GREEN_TG_ARN,containerName=latticedb,containerPort=8080" \
        --health-check-grace-period-seconds 60 \
        --tags "key=Environment,value=$ENVIRONMENT" "key=DeploymentType,value=green" \
        > /dev/null

    log_success "Green service created: $GREEN_SERVICE_NAME"
}

# Wait for green service to be healthy
wait_for_green_healthy() {
    log "Waiting for green service to become healthy..."

    local retries=0
    local max_retries=$HEALTH_CHECK_RETRIES
    local interval=$HEALTH_CHECK_INTERVAL

    while [[ $retries -lt $max_retries ]]; do
        # Check service stability
        RUNNING_COUNT=$(aws ecs describe-services \
            --region "$AWS_REGION" \
            --cluster "$CLUSTER_NAME" \
            --services "$GREEN_SERVICE_NAME" \
            --query 'services[0].runningCount' \
            --output text)

        DESIRED_COUNT=$(aws ecs describe-services \
            --region "$AWS_REGION" \
            --cluster "$CLUSTER_NAME" \
            --services "$GREEN_SERVICE_NAME" \
            --query 'services[0].desiredCount' \
            --output text)

        # Check target group health
        HEALTHY_COUNT=$(aws elbv2 describe-target-health \
            --region "$AWS_REGION" \
            --target-group-arn "$GREEN_TG_ARN" \
            --query 'length(TargetHealthDescriptions[?TargetHealth.State==`healthy`])' \
            --output text)

        log "Green service status: $RUNNING_COUNT/$DESIRED_COUNT tasks running, $HEALTHY_COUNT healthy targets"

        if [[ "$RUNNING_COUNT" == "$DESIRED_COUNT" ]] && [[ "$HEALTHY_COUNT" -ge "$DESIRED_COUNT" ]]; then
            log_success "Green service is healthy and ready!"
            return 0
        fi

        retries=$((retries + 1))
        if [[ $retries -lt $max_retries ]]; then
            log "Retry $retries/$max_retries - waiting ${interval}s..."
            sleep "$interval"
        fi
    done

    log_error "Green service failed to become healthy within timeout"
    return 1
}

# Switch traffic to green
switch_traffic_to_green() {
    log "Switching traffic to green version..."

    # Get listener rules from blue target group
    LISTENER_ARNS=$(aws elbv2 describe-target-groups \
        --region "$AWS_REGION" \
        --target-group-arns "$BLUE_TG_ARN" \
        --query 'TargetGroups[0].LoadBalancerArns[]' \
        --output text)

    for LB_ARN in $LISTENER_ARNS; do
        LISTENERS=$(aws elbv2 describe-listeners \
            --region "$AWS_REGION" \
            --load-balancer-arn "$LB_ARN" \
            --query 'Listeners[*].ListenerArn' \
            --output text)

        for LISTENER_ARN in $LISTENERS; do
            log "Updating listener: $LISTENER_ARN"

            # Update default action to point to green target group
            aws elbv2 modify-listener \
                --region "$AWS_REGION" \
                --listener-arn "$LISTENER_ARN" \
                --default-actions "Type=forward,TargetGroupArn=$GREEN_TG_ARN" \
                > /dev/null
        done
    done

    log_success "Traffic switched to green target group"
}

# Validate green in production
validate_green_production() {
    log "Validating green version in production..."

    # Get load balancer DNS
    LB_DNS=$(aws elbv2 describe-load-balancers \
        --region "$AWS_REGION" \
        --load-balancer-arns $(aws elbv2 describe-target-groups \
            --region "$AWS_REGION" \
            --target-group-arns "$GREEN_TG_ARN" \
            --query 'TargetGroups[0].LoadBalancerArns[0]' \
            --output text) \
        --query 'LoadBalancers[0].DNSName' \
        --output text)

    log "Testing health endpoint: http://${LB_DNS}${HEALTH_CHECK_PATH}"

    local retries=0
    while [[ $retries -lt 5 ]]; do
        if curl -sf "http://${LB_DNS}${HEALTH_CHECK_PATH}" > /dev/null; then
            log_success "Green version is serving traffic successfully!"
            return 0
        fi
        retries=$((retries + 1))
        sleep 5
    done

    log_error "Green version failed production validation"
    return 1
}

# Cleanup blue version
cleanup_blue_version() {
    log "Cleaning up blue version..."

    # Delete blue service
    aws ecs update-service \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --service "$SERVICE_NAME" \
        --desired-count 0 \
        > /dev/null

    log "Waiting for blue service to drain..."
    sleep 30

    aws ecs delete-service \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --service "$SERVICE_NAME" \
        > /dev/null

    # Rename green service to primary
    log "Promoting green service to primary..."

    # Note: ECS doesn't support renaming, so we keep the green service as-is
    # and document it as the active service

    log_success "Blue version cleaned up successfully"
}

# Rollback to blue
rollback_to_blue() {
    log_error "Rolling back to blue version..."

    # Switch traffic back to blue
    LISTENER_ARNS=$(aws elbv2 describe-target-groups \
        --region "$AWS_REGION" \
        --target-group-arns "$BLUE_TG_ARN" \
        --query 'TargetGroups[0].LoadBalancerArns[]' \
        --output text)

    for LB_ARN in $LISTENER_ARNS; do
        LISTENERS=$(aws elbv2 describe-listeners \
            --region "$AWS_REGION" \
            --load-balancer-arn "$LB_ARN" \
            --query 'Listeners[*].ListenerArn' \
            --output text)

        for LISTENER_ARN in $LISTENERS; do
            aws elbv2 modify-listener \
                --region "$AWS_REGION" \
                --listener-arn "$LISTENER_ARN" \
                --default-actions "Type=forward,TargetGroupArn=$BLUE_TG_ARN" \
                > /dev/null
        done
    done

    log "Traffic switched back to blue"

    # Delete green service
    if [[ -n "${GREEN_SERVICE_NAME:-}" ]]; then
        aws ecs update-service \
            --region "$AWS_REGION" \
            --cluster "$CLUSTER_NAME" \
            --service "$GREEN_SERVICE_NAME" \
            --desired-count 0 \
            > /dev/null

        sleep 20

        aws ecs delete-service \
            --region "$AWS_REGION" \
            --cluster "$CLUSTER_NAME" \
            --service "$GREEN_SERVICE_NAME" \
            > /dev/null

        log "Green service deleted"
    fi

    # Delete green target group
    if [[ -n "${GREEN_TG_ARN:-}" ]]; then
        aws elbv2 delete-target-group \
            --region "$AWS_REGION" \
            --target-group-arn "$GREEN_TG_ARN" \
            > /dev/null

        log "Green target group deleted"
    fi

    log_error "Rollback completed - blue version is active"
    exit 1
}

# Manual approval prompt
request_approval() {
    if [[ "$APPROVAL_REQUIRED" == "true" ]]; then
        log_warning "==================================================================="
        log_warning "Green version is healthy and ready to receive production traffic"
        log_warning "==================================================================="
        log_warning "Green Service: $GREEN_SERVICE_NAME"
        log_warning "Green Target Group: $GREEN_TG_ARN"
        log_warning "New Task Definition: $NEW_TASK_DEF_ARN"
        log_warning "Image: $IMAGE_URI"
        log_warning ""
        log_warning "This will switch 100% of production traffic to the new version."
        log_warning ""
        read -p "Do you want to proceed with traffic switch? (yes/no): " -r
        echo

        if [[ ! $REPLY =~ ^[Yy]es$ ]]; then
            log_warning "Deployment cancelled by user"
            rollback_to_blue
        fi
    else
        log "Auto-approval enabled - proceeding with traffic switch"
    fi
}

# Main deployment flow
main() {
    log "========================================"
    log "Blue/Green Deployment Started"
    log "========================================"
    log "Environment: $ENVIRONMENT"
    log "Cluster: $CLUSTER_NAME"
    log "Service: $SERVICE_NAME"
    log "Image: $IMAGE_URI"
    log "========================================"

    # Deployment steps with error handling
    get_current_task_definition || { log_error "Failed to get current task definition"; exit 1; }
    create_new_task_definition || { log_error "Failed to create new task definition"; exit 1; }
    get_target_groups || { log_error "Failed to get target groups"; exit 1; }
    create_green_target_group || { log_error "Failed to create green target group"; exit 1; }
    deploy_green_version || { log_error "Failed to deploy green version"; rollback_to_blue; }

    if ! wait_for_green_healthy; then
        if [[ "$ROLLBACK_ON_FAILURE" == "true" ]]; then
            rollback_to_blue
        else
            log_error "Green version is unhealthy but rollback is disabled"
            exit 1
        fi
    fi

    request_approval
    switch_traffic_to_green || { log_error "Failed to switch traffic"; rollback_to_blue; }

    if ! validate_green_production; then
        if [[ "$ROLLBACK_ON_FAILURE" == "true" ]]; then
            rollback_to_blue
        else
            log_error "Green version failed validation but rollback is disabled"
            exit 1
        fi
    fi

    cleanup_blue_version

    log_success "========================================"
    log_success "Blue/Green Deployment Completed Successfully!"
    log_success "========================================"
    log_success "Active Service: $GREEN_SERVICE_NAME"
    log_success "Active Target Group: $GREEN_TG_ARN"
    log_success "Task Definition: $NEW_TASK_DEF_ARN"
    log_success "Image: $IMAGE_URI"
    log_success "========================================"
}

# Run main function
main
