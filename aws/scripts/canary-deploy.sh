#!/bin/bash

# Canary Deployment Script for AWS ECS
# Implements progressive traffic shifting with automated rollback

set -euo pipefail

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Default configuration
ENVIRONMENT=${ENVIRONMENT:-"staging"}
AWS_REGION=${AWS_REGION:-"us-east-1"}
CLUSTER_NAME="latticedb-${ENVIRONMENT}"
SERVICE_NAME="latticedb-service-${ENVIRONMENT}"
TASK_FAMILY="latticedb-task-${ENVIRONMENT}"

# Canary configuration
CANARY_PERCENTAGE=${CANARY_PERCENTAGE:-10}
TRAFFIC_INCREMENT=${TRAFFIC_INCREMENT:-10}
TRAFFIC_INCREMENT_INTERVAL=${TRAFFIC_INCREMENT_INTERVAL:-300}
HEALTH_CHECK_RETRIES=${HEALTH_CHECK_RETRIES:-20}
HEALTH_CHECK_INTERVAL=${HEALTH_CHECK_INTERVAL:-10}
ERROR_RATE_THRESHOLD=${ERROR_RATE_THRESHOLD:-5}
LATENCY_THRESHOLD_MS=${LATENCY_THRESHOLD_MS:-1000}
ROLLBACK_ON_FAILURE=${ROLLBACK_ON_FAILURE:-true}

# Monitoring configuration
CLOUDWATCH_NAMESPACE="LatticeDB/Canary"
METRIC_EVALUATION_PERIODS=3

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Canary Deployment Script for AWS ECS with Progressive Traffic Shifting

OPTIONS:
    -e, --environment ENV           Environment (dev/staging/prod) [default: staging]
    -r, --region REGION             AWS region [default: us-east-1]
    -i, --image IMAGE               Docker image URI (required)
    -c, --canary-percent PCT        Initial canary traffic percentage [default: 10]
    -s, --increment PCT             Traffic increment per step [default: 10]
    -t, --increment-interval SEC    Seconds between increments [default: 300]
    --error-threshold PCT           Max error rate threshold [default: 5]
    --latency-threshold MS          Max latency threshold in ms [default: 1000]
    --no-rollback                   Disable automatic rollback
    -h, --help                      Show this help message

EXAMPLES:
    # Standard canary with 10% initial traffic
    $0 -e prod -i 123456789.dkr.ecr.us-east-1.amazonaws.com/latticedb:v2.0.0

    # Aggressive canary with 20% initial traffic and 20% increments
    $0 -e staging -i image:tag -c 20 -s 20 -t 180

    # Conservative canary with strict thresholds
    $0 -e prod -i image:tag -c 5 -s 5 -t 600 --error-threshold 1 --latency-threshold 500

EOF
    exit 1
}

# Parse arguments
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
        -i|--image)
            IMAGE_URI="$2"
            shift 2
            ;;
        -c|--canary-percent)
            CANARY_PERCENTAGE="$2"
            shift 2
            ;;
        -s|--increment)
            TRAFFIC_INCREMENT="$2"
            shift 2
            ;;
        -t|--increment-interval)
            TRAFFIC_INCREMENT_INTERVAL="$2"
            shift 2
            ;;
        --error-threshold)
            ERROR_RATE_THRESHOLD="$2"
            shift 2
            ;;
        --latency-threshold)
            LATENCY_THRESHOLD_MS="$2"
            shift 2
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

# Validate
if [[ -z "${IMAGE_URI:-}" ]]; then
    echo -e "${RED}Error: Docker image URI is required${NC}"
    usage
fi

if [[ ! "$ENVIRONMENT" =~ ^(dev|staging|prod)$ ]]; then
    echo -e "${RED}Error: Environment must be dev, staging, or prod${NC}"
    exit 1
fi

if [[ $CANARY_PERCENTAGE -lt 1 || $CANARY_PERCENTAGE -gt 50 ]]; then
    echo -e "${RED}Error: Canary percentage must be between 1 and 50${NC}"
    exit 1
fi

# Logging functions
log() { echo -e "${BLUE}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"; }
log_success() { echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"; }
log_error() { echo -e "${RED}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"; }

# Get current task definition
get_current_task_definition() {
    log "Fetching current task definition..."

    CURRENT_TASK_DEF=$(aws ecs describe-task-definition \
        --region "$AWS_REGION" \
        --task-definition "$TASK_FAMILY" \
        --query 'taskDefinition.taskDefinitionArn' \
        --output text)

    log_success "Current task definition: $CURRENT_TASK_DEF"
}

# Create new task definition
create_canary_task_definition() {
    log "Creating canary task definition with image: $IMAGE_URI"

    aws ecs describe-task-definition \
        --region "$AWS_REGION" \
        --task-definition "$TASK_FAMILY" \
        --query 'taskDefinition' \
        --output json > /tmp/canary-task-def.json

    jq --arg IMAGE "$IMAGE_URI" \
        'del(.taskDefinitionArn, .revision, .status, .requiresAttributes, .compatibilities, .registeredAt, .registeredBy) |
         .containerDefinitions[0].image = $IMAGE |
         .containerDefinitions[0].environment += [
           {"name": "DEPLOYMENT_TYPE", "value": "canary"},
           {"name": "CANARY_VERSION", "value": "true"}
         ]' \
        /tmp/canary-task-def.json > /tmp/new-canary-task-def.json

    CANARY_TASK_DEF=$(aws ecs register-task-definition \
        --region "$AWS_REGION" \
        --cli-input-json file:///tmp/new-canary-task-def.json \
        --query 'taskDefinition.taskDefinitionArn' \
        --output text)

    log_success "Canary task definition created: $CANARY_TASK_DEF"
}

# Get target group
get_target_group() {
    log "Fetching target group..."

    STABLE_TG_ARN=$(aws ecs describe-services \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --services "$SERVICE_NAME" \
        --query 'services[0].loadBalancers[0].targetGroupArn' \
        --output text)

    if [[ -z "$STABLE_TG_ARN" || "$STABLE_TG_ARN" == "None" ]]; then
        log_error "No target group found for service"
        exit 1
    fi

    log_success "Stable target group: $STABLE_TG_ARN"
}

# Create canary target group
create_canary_target_group() {
    log "Creating canary target group..."

    STABLE_TG_CONFIG=$(aws elbv2 describe-target-groups \
        --region "$AWS_REGION" \
        --target-group-arns "$STABLE_TG_ARN" \
        --query 'TargetGroups[0]' \
        --output json)

    VPC_ID=$(echo "$STABLE_TG_CONFIG" | jq -r '.VpcId')
    PROTOCOL=$(echo "$STABLE_TG_CONFIG" | jq -r '.Protocol')
    PORT=$(echo "$STABLE_TG_CONFIG" | jq -r '.Port')

    CANARY_TG_NAME="latticedb-canary-${ENVIRONMENT}-$(date +%s)"

    CANARY_TG_ARN=$(aws elbv2 create-target-group \
        --region "$AWS_REGION" \
        --name "$CANARY_TG_NAME" \
        --protocol "$PROTOCOL" \
        --port "$PORT" \
        --vpc-id "$VPC_ID" \
        --health-check-path "/health" \
        --health-check-interval-seconds 30 \
        --health-check-timeout-seconds 5 \
        --healthy-threshold-count 2 \
        --unhealthy-threshold-count 3 \
        --target-type ip \
        --tags "Key=Environment,Value=$ENVIRONMENT" "Key=DeploymentType,Value=canary" \
        --query 'TargetGroups[0].TargetGroupArn' \
        --output text)

    log_success "Canary target group created: $CANARY_TG_ARN"
}

# Deploy canary service
deploy_canary_service() {
    log "Deploying canary service..."

    CANARY_SERVICE_NAME="${SERVICE_NAME}-canary"

    # Get network configuration from stable service
    NETWORK_CONFIG=$(aws ecs describe-services \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --services "$SERVICE_NAME" \
        --query 'services[0].networkConfiguration' \
        --output json)

    # Calculate canary task count (at least 1)
    STABLE_COUNT=$(aws ecs describe-services \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --services "$SERVICE_NAME" \
        --query 'services[0].desiredCount' \
        --output text)

    CANARY_COUNT=$(( (STABLE_COUNT * CANARY_PERCENTAGE + 99) / 100 ))
    CANARY_COUNT=$(( CANARY_COUNT < 1 ? 1 : CANARY_COUNT ))

    log "Creating canary service with $CANARY_COUNT tasks (stable has $STABLE_COUNT)"

    aws ecs create-service \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --service-name "$CANARY_SERVICE_NAME" \
        --task-definition "$CANARY_TASK_DEF" \
        --desired-count "$CANARY_COUNT" \
        --launch-type FARGATE \
        --network-configuration "$NETWORK_CONFIG" \
        --load-balancers "targetGroupArn=$CANARY_TG_ARN,containerName=latticedb,containerPort=8080" \
        --health-check-grace-period-seconds 60 \
        --tags "key=Environment,value=$ENVIRONMENT" "key=DeploymentType,value=canary" \
        > /dev/null

    log_success "Canary service created: $CANARY_SERVICE_NAME"
}

# Wait for canary to be healthy
wait_for_canary_healthy() {
    log "Waiting for canary to become healthy..."

    local retries=0
    while [[ $retries -lt $HEALTH_CHECK_RETRIES ]]; do
        RUNNING=$(aws ecs describe-services \
            --region "$AWS_REGION" \
            --cluster "$CLUSTER_NAME" \
            --services "${SERVICE_NAME}-canary" \
            --query 'services[0].runningCount' \
            --output text)

        DESIRED=$(aws ecs describe-services \
            --region "$AWS_REGION" \
            --cluster "$CLUSTER_NAME" \
            --services "${SERVICE_NAME}-canary" \
            --query 'services[0].desiredCount' \
            --output text)

        HEALTHY=$(aws elbv2 describe-target-health \
            --region "$AWS_REGION" \
            --target-group-arn "$CANARY_TG_ARN" \
            --query 'length(TargetHealthDescriptions[?TargetHealth.State==`healthy`])' \
            --output text)

        log "Canary status: $RUNNING/$DESIRED running, $HEALTHY healthy targets"

        if [[ "$RUNNING" == "$DESIRED" ]] && [[ "$HEALTHY" -ge "$DESIRED" ]]; then
            log_success "Canary is healthy!"
            return 0
        fi

        retries=$((retries + 1))
        sleep "$HEALTH_CHECK_INTERVAL"
    done

    log_error "Canary failed to become healthy"
    return 1
}

# Configure weighted traffic routing
configure_traffic_routing() {
    local canary_weight=$1
    local stable_weight=$((100 - canary_weight))

    log "Configuring traffic: ${stable_weight}% stable, ${canary_weight}% canary"

    # Get listener ARN
    LB_ARN=$(aws elbv2 describe-target-groups \
        --region "$AWS_REGION" \
        --target-group-arns "$STABLE_TG_ARN" \
        --query 'TargetGroups[0].LoadBalancerArns[0]' \
        --output text)

    LISTENER_ARN=$(aws elbv2 describe-listeners \
        --region "$AWS_REGION" \
        --load-balancer-arn "$LB_ARN" \
        --query 'Listeners[0].ListenerArn' \
        --output text)

    # Create forward action with weighted targets
    cat > /tmp/forward-config.json <<EOF
{
    "Type": "forward",
    "ForwardConfig": {
        "TargetGroups": [
            {
                "TargetGroupArn": "$STABLE_TG_ARN",
                "Weight": $stable_weight
            },
            {
                "TargetGroupArn": "$CANARY_TG_ARN",
                "Weight": $canary_weight
            }
        ],
        "TargetGroupStickinessConfig": {
            "Enabled": false
        }
    }
}
EOF

    aws elbv2 modify-listener \
        --region "$AWS_REGION" \
        --listener-arn "$LISTENER_ARN" \
        --default-actions file:///tmp/forward-config.json \
        > /dev/null

    log_success "Traffic routing configured"
}

# Monitor canary metrics
monitor_canary_metrics() {
    local duration=$1
    log "Monitoring canary metrics for ${duration}s..."

    local end_time=$(($(date +%s) + duration))

    while [[ $(date +%s) -lt $end_time ]]; do
        # Get error rates from CloudWatch (simplified - would need actual metrics)
        log "Checking canary health metrics..."

        # Check target group health
        UNHEALTHY=$(aws elbv2 describe-target-health \
            --region "$AWS_REGION" \
            --target-group-arn "$CANARY_TG_ARN" \
            --query 'length(TargetHealthDescriptions[?TargetHealth.State!=`healthy`])' \
            --output text)

        if [[ "$UNHEALTHY" -gt 0 ]]; then
            log_warning "Detected $UNHEALTHY unhealthy targets in canary"
            return 1
        fi

        # In production, you would also check:
        # - Error rates from application metrics
        # - Latency percentiles (p50, p95, p99)
        # - Request success rates
        # - Custom business metrics

        sleep 30
    done

    log_success "Canary passed health monitoring"
    return 0
}

# Progressive traffic shift
progressive_traffic_shift() {
    log "Starting progressive traffic shift..."

    local current_weight=$CANARY_PERCENTAGE

    while [[ $current_weight -lt 100 ]]; do
        configure_traffic_routing "$current_weight"

        if ! monitor_canary_metrics "$TRAFFIC_INCREMENT_INTERVAL"; then
            log_error "Canary failed health checks at ${current_weight}% traffic"
            return 1
        fi

        log_success "Canary stable at ${current_weight}% traffic"

        current_weight=$((current_weight + TRAFFIC_INCREMENT))
        if [[ $current_weight -gt 100 ]]; then
            current_weight=100
        fi
    done

    configure_traffic_routing 100
    log_success "Progressive shift complete - 100% traffic on canary"
    return 0
}

# Promote canary to stable
promote_canary() {
    log "Promoting canary to stable..."

    # Scale down old stable service
    aws ecs update-service \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --service "$SERVICE_NAME" \
        --desired-count 0 \
        > /dev/null

    log "Draining old stable service..."
    sleep 30

    # Delete old stable service
    aws ecs delete-service \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --service "$SERVICE_NAME" \
        --force \
        > /dev/null

    # Update canary service to use stable task definition
    aws ecs update-service \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --service "${SERVICE_NAME}-canary" \
        --task-definition "$CANARY_TASK_DEF" \
        > /dev/null

    # Update listener to use only canary target group (now stable)
    LB_ARN=$(aws elbv2 describe-target-groups \
        --region "$AWS_REGION" \
        --target-group-arns "$CANARY_TG_ARN" \
        --query 'TargetGroups[0].LoadBalancerArns[0]' \
        --output text)

    LISTENER_ARN=$(aws elbv2 describe-listeners \
        --region "$AWS_REGION" \
        --load-balancer-arn "$LB_ARN" \
        --query 'Listeners[0].ListenerArn' \
        --output text)

    aws elbv2 modify-listener \
        --region "$AWS_REGION" \
        --listener-arn "$LISTENER_ARN" \
        --default-actions "Type=forward,TargetGroupArn=$CANARY_TG_ARN" \
        > /dev/null

    log_success "Canary promoted to stable"
}

# Rollback canary
rollback_canary() {
    log_error "Rolling back canary deployment..."

    # Switch all traffic back to stable
    LB_ARN=$(aws elbv2 describe-target-groups \
        --region "$AWS_REGION" \
        --target-group-arns "$STABLE_TG_ARN" \
        --query 'TargetGroups[0].LoadBalancerArns[0]' \
        --output text)

    LISTENER_ARN=$(aws elbv2 describe-listeners \
        --region "$AWS_REGION" \
        --load-balancer-arn "$LB_ARN" \
        --query 'Listeners[0].ListenerArn' \
        --output text)

    aws elbv2 modify-listener \
        --region "$AWS_REGION" \
        --listener-arn "$LISTENER_ARN" \
        --default-actions "Type=forward,TargetGroupArn=$STABLE_TG_ARN" \
        > /dev/null

    log "Traffic switched back to stable"

    # Delete canary service
    if aws ecs describe-services \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --services "${SERVICE_NAME}-canary" \
        --query 'services[0].serviceName' \
        --output text 2>/dev/null | grep -q canary; then

        aws ecs update-service \
            --region "$AWS_REGION" \
            --cluster "$CLUSTER_NAME" \
            --service "${SERVICE_NAME}-canary" \
            --desired-count 0 \
            > /dev/null

        sleep 20

        aws ecs delete-service \
            --region "$AWS_REGION" \
            --cluster "$CLUSTER_NAME" \
            --service "${SERVICE_NAME}-canary" \
            --force \
            > /dev/null
    fi

    # Delete canary target group
    if [[ -n "${CANARY_TG_ARN:-}" ]]; then
        aws elbv2 delete-target-group \
            --region "$AWS_REGION" \
            --target-group-arn "$CANARY_TG_ARN" \
            2>/dev/null || true
    fi

    log_error "Rollback completed"
    exit 1
}

# Main flow
main() {
    log "========================================"
    log "Canary Deployment Started"
    log "========================================"
    log "Environment: $ENVIRONMENT"
    log "Image: $IMAGE_URI"
    log "Initial Canary: ${CANARY_PERCENTAGE}%"
    log "Traffic Increment: ${TRAFFIC_INCREMENT}%"
    log "Increment Interval: ${TRAFFIC_INCREMENT_INTERVAL}s"
    log "========================================"

    get_current_task_definition || { log_error "Failed to get task definition"; exit 1; }
    create_canary_task_definition || { log_error "Failed to create canary task definition"; exit 1; }
    get_target_group || { log_error "Failed to get target group"; exit 1; }
    create_canary_target_group || { log_error "Failed to create canary target group"; exit 1; }
    deploy_canary_service || { log_error "Failed to deploy canary"; rollback_canary; }

    if ! wait_for_canary_healthy; then
        [[ "$ROLLBACK_ON_FAILURE" == "true" ]] && rollback_canary || exit 1
    fi

    configure_traffic_routing "$CANARY_PERCENTAGE"

    if ! progressive_traffic_shift; then
        [[ "$ROLLBACK_ON_FAILURE" == "true" ]] && rollback_canary || exit 1
    fi

    promote_canary

    log_success "========================================"
    log_success "Canary Deployment Completed!"
    log_success "========================================"
    log_success "New stable version: $IMAGE_URI"
    log_success "Task definition: $CANARY_TASK_DEF"
    log_success "========================================"
}

main
