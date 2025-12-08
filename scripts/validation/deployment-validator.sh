#!/bin/bash

# Deployment Validator
# Comprehensive validation framework for deployments

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
ENVIRONMENT=${ENVIRONMENT:-"staging"}
AWS_REGION=${AWS_REGION:-"us-east-1"}
VALIDATION_TIMEOUT=${VALIDATION_TIMEOUT:-600}
METRIC_WINDOW=${METRIC_WINDOW:-300}

# Thresholds
MAX_ERROR_RATE=5.0
MAX_P50_LATENCY_MS=200
MAX_P95_LATENCY_MS=1000
MAX_P99_LATENCY_MS=2000
MIN_SUCCESS_RATE=95.0
MAX_CPU_PERCENT=80
MAX_MEMORY_PERCENT=85
MIN_HEALTHY_TASKS=1

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Deployment Validator - Validates deployment health and metrics

OPTIONS:
    -e, --environment ENV       Environment (dev/staging/prod)
    -r, --region REGION         AWS region
    --timeout SECONDS          Validation timeout [default: 600]
    --metric-window SECONDS    Metric collection window [default: 300]
    -h, --help                 Show this help

EXAMPLES:
    $0 -e prod -r us-east-1
    $0 -e staging --timeout 300

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
        --timeout)
            VALIDATION_TIMEOUT="$2"
            shift 2
            ;;
        --metric-window)
            METRIC_WINDOW="$2"
            shift 2
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

# Logging
log() { echo -e "${BLUE}[VALIDATOR]${NC} $1"; }
log_success() { echo -e "${GREEN}[PASS]${NC} $1"; }
log_error() { echo -e "${RED}[FAIL]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARN]${NC} $1"; }

CLUSTER_NAME="latticedb-${ENVIRONMENT}"
SERVICE_NAME="latticedb-service-${ENVIRONMENT}"

# Validate ECS service health
validate_ecs_service() {
    log "Validating ECS service health..."

    local running=$(aws ecs describe-services \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --services "$SERVICE_NAME" \
        --query 'services[0].runningCount' \
        --output text)

    local desired=$(aws ecs describe-services \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --services "$SERVICE_NAME" \
        --query 'services[0].desiredCount' \
        --output text)

    if [[ "$running" == "$desired" ]] && [[ "$running" -ge "$MIN_HEALTHY_TASKS" ]]; then
        log_success "ECS service healthy: $running/$desired tasks running"
        return 0
    else
        log_error "ECS service unhealthy: $running/$desired tasks running"
        return 1
    fi
}

# Validate target group health
validate_target_group() {
    log "Validating target group health..."

    local tg_arn=$(aws ecs describe-services \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --services "$SERVICE_NAME" \
        --query 'services[0].loadBalancers[0].targetGroupArn' \
        --output text)

    if [[ -z "$tg_arn" || "$tg_arn" == "None" ]]; then
        log_warning "No target group configured"
        return 0
    fi

    local healthy=$(aws elbv2 describe-target-health \
        --region "$AWS_REGION" \
        --target-group-arn "$tg_arn" \
        --query 'length(TargetHealthDescriptions[?TargetHealth.State==`healthy`])' \
        --output text)

    local total=$(aws elbv2 describe-target-health \
        --region "$AWS_REGION" \
        --target-group-arn "$tg_arn" \
        --query 'length(TargetHealthDescriptions)' \
        --output text)

    if [[ "$healthy" -ge "$MIN_HEALTHY_TASKS" ]]; then
        log_success "Target group healthy: $healthy/$total targets healthy"
        return 0
    else
        log_error "Target group unhealthy: $healthy/$total targets healthy"
        return 1
    fi
}

# Validate CloudWatch metrics
validate_cloudwatch_metrics() {
    log "Validating CloudWatch metrics (${METRIC_WINDOW}s window)..."

    local end_time=$(date -u +"%Y-%m-%dT%H:%M:%S")
    local start_time=$(date -u -d "$METRIC_WINDOW seconds ago" +"%Y-%m-%dT%H:%M:%S" 2>/dev/null || \
                       date -u -v-${METRIC_WINDOW}S +"%Y-%m-%dT%H:%M:%S")

    # CPU Utilization
    local cpu_avg=$(aws cloudwatch get-metric-statistics \
        --region "$AWS_REGION" \
        --namespace "AWS/ECS" \
        --metric-name "CPUUtilization" \
        --dimensions "Name=ServiceName,Value=$SERVICE_NAME" "Name=ClusterName,Value=$CLUSTER_NAME" \
        --start-time "$start_time" \
        --end-time "$end_time" \
        --period 60 \
        --statistics Average \
        --query 'Datapoints[*].Average' \
        --output json 2>/dev/null | jq -r 'add/length // 0')

    if (( $(echo "$cpu_avg > 0" | bc -l) )); then
        if (( $(echo "$cpu_avg > $MAX_CPU_PERCENT" | bc -l) )); then
            log_warning "High CPU usage: ${cpu_avg}%"
        else
            log_success "CPU usage: ${cpu_avg}%"
        fi
    fi

    # Memory Utilization
    local mem_avg=$(aws cloudwatch get-metric-statistics \
        --region "$AWS_REGION" \
        --namespace "AWS/ECS" \
        --metric-name "MemoryUtilization" \
        --dimensions "Name=ServiceName,Value=$SERVICE_NAME" "Name=ClusterName,Value=$CLUSTER_NAME" \
        --start-time "$start_time" \
        --end-time "$end_time" \
        --period 60 \
        --statistics Average \
        --query 'Datapoints[*].Average' \
        --output json 2>/dev/null | jq -r 'add/length // 0')

    if (( $(echo "$mem_avg > 0" | bc -l) )); then
        if (( $(echo "$mem_avg > $MAX_MEMORY_PERCENT" | bc -l) )); then
            log_warning "High memory usage: ${mem_avg}%"
        else
            log_success "Memory usage: ${mem_avg}%"
        fi
    fi

    return 0
}

# Run smoke tests
run_smoke_tests() {
    log "Running smoke tests..."

    if [[ -f "./scripts/validation/smoke-tests.sh" ]]; then
        if ./scripts/validation/smoke-tests.sh -u "$TARGET_URL"; then
            log_success "Smoke tests passed"
            return 0
        else
            log_error "Smoke tests failed"
            return 1
        fi
    else
        log_warning "Smoke test script not found, skipping"
        return 0
    fi
}

# Check for recent errors in logs
check_error_logs() {
    log "Checking for errors in logs..."

    local log_group="/ecs/${CLUSTER_NAME}-${SERVICE_NAME}"
    local error_count=$(aws logs filter-log-events \
        --region "$AWS_REGION" \
        --log-group-name "$log_group" \
        --start-time $(( $(date +%s) - 300 ))000 \
        --filter-pattern "ERROR" \
        --query 'length(events)' \
        --output text 2>/dev/null || echo "0")

    if [[ "$error_count" -gt 100 ]]; then
        log_warning "High error count in logs: $error_count errors in last 5 minutes"
    else
        log_success "Error log count acceptable: $error_count errors in last 5 minutes"
    fi

    return 0
}

# Validate deployment
main() {
    log "========================================"
    log "Deployment Validation"
    log "========================================"
    log "Environment: $ENVIRONMENT"
    log "Cluster: $CLUSTER_NAME"
    log "Service: $SERVICE_NAME"
    log "========================================"

    local failed=false

    # Get endpoint
    local lb_dns=$(aws elbv2 describe-load-balancers \
        --region "$AWS_REGION" \
        --query "LoadBalancers[?contains(LoadBalancerName, 'latticedb')].DNSName" \
        --output text 2>/dev/null | head -n1)

    if [[ -n "$lb_dns" ]]; then
        TARGET_URL="http://${lb_dns}"
        log "Target URL: $TARGET_URL"
    else
        TARGET_URL="http://localhost:8080"
        log_warning "Could not determine load balancer DNS, using localhost"
    fi

    # Run validations
    validate_ecs_service || failed=true
    validate_target_group || failed=true
    validate_cloudwatch_metrics || failed=true
    check_error_logs || failed=true
    run_smoke_tests || failed=true

    log "========================================"

    if [[ "$failed" == "true" ]]; then
        log_error "Deployment validation FAILED"
        exit 1
    else
        log_success "Deployment validation PASSED"
        exit 0
    fi
}

main
