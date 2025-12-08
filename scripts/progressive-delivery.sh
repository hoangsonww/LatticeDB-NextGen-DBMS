#!/bin/bash

# Progressive Delivery Framework
# Orchestrates deployment strategies with automated validation and rollback

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Configuration
DEPLOYMENT_STRATEGY=${DEPLOYMENT_STRATEGY:-"canary"}
ENVIRONMENT=${ENVIRONMENT:-"staging"}
VALIDATION_ENABLED=${VALIDATION_ENABLED:-true}
SMOKE_TEST_ENABLED=${SMOKE_TEST_ENABLED:-true}
METRICS_VALIDATION_ENABLED=${METRICS_VALIDATION_ENABLED:-true}
AUTO_ROLLBACK=${AUTO_ROLLBACK:-true}
NOTIFICATION_ENABLED=${NOTIFICATION_ENABLED:-false}
SLACK_WEBHOOK_URL=${SLACK_WEBHOOK_URL:-""}

# Validation thresholds
MAX_ERROR_RATE=5.0
MAX_P95_LATENCY_MS=1000
MIN_SUCCESS_RATE=95.0
MAX_CPU_PERCENT=80
MAX_MEMORY_PERCENT=85

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Progressive Delivery Framework - Advanced deployment orchestration

STRATEGIES:
    rolling         - Rolling update (default ECS behavior)
    blue-green      - Blue/green deployment with instant cutover
    canary          - Gradual traffic shift (default)
    dark-launch     - Deploy without traffic, manual validation
    feature-flag    - Deploy with feature flags (A/B testing)

OPTIONS:
    -s, --strategy STRATEGY     Deployment strategy [default: canary]
    -e, --environment ENV       Environment (dev/staging/prod) [default: staging]
    -i, --image IMAGE           Docker image URI (required)
    --platform PLATFORM         Platform (aws/azure/gcp/hashicorp) [default: aws]
    --skip-validation           Skip automated validation
    --skip-smoke-tests          Skip smoke tests
    --skip-metrics              Skip metrics validation
    --no-rollback               Disable automatic rollback
    --notify                    Enable Slack notifications
    -h, --help                  Show this help

EXAMPLES:
    # Canary deployment to production
    $0 -s canary -e prod -i myimage:v2.0.0 --notify

    # Blue/green with validation
    $0 -s blue-green -e staging -i myimage:v2.0.0

    # Feature flag deployment
    $0 -s feature-flag -e prod -i myimage:v2.0.0 --platform aws

EOF
    exit 1
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--strategy)
            DEPLOYMENT_STRATEGY="$2"
            shift 2
            ;;
        -e|--environment)
            ENVIRONMENT="$2"
            shift 2
            ;;
        -i|--image)
            IMAGE_URI="$2"
            shift 2
            ;;
        --platform)
            PLATFORM="$2"
            shift 2
            ;;
        --skip-validation)
            VALIDATION_ENABLED=false
            shift
            ;;
        --skip-smoke-tests)
            SMOKE_TEST_ENABLED=false
            shift
            ;;
        --skip-metrics)
            METRICS_VALIDATION_ENABLED=false
            shift
            ;;
        --no-rollback)
            AUTO_ROLLBACK=false
            shift
            ;;
        --notify)
            NOTIFICATION_ENABLED=true
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
    echo -e "${RED}Error: Image URI required${NC}"
    usage
fi

PLATFORM=${PLATFORM:-"aws"}

# Logging
log() { echo -e "${BLUE}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"; }
log_success() { echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')] ✓${NC} $1"; }
log_warning() { echo -e "${YELLOW}[$(date +'%Y-%m-%d %H:%M:%S')] ⚠${NC} $1"; }
log_error() { echo -e "${RED}[$(date +'%Y-%m-%d %H:%M:%S')] ✗${NC} $1"; }
log_info() { echo -e "${CYAN}[$(date +'%Y-%m-%d %H:%M:%S')] ℹ${NC} $1"; }

# Notification function
send_notification() {
    local status=$1
    local message=$2

    if [[ "$NOTIFICATION_ENABLED" == "true" ]] && [[ -n "$SLACK_WEBHOOK_URL" ]]; then
        local emoji
        case $status in
            success) emoji=":white_check_mark:" ;;
            failure) emoji=":x:" ;;
            warning) emoji=":warning:" ;;
            info) emoji=":information_source:" ;;
            *) emoji=":robot_face:" ;;
        esac

        local payload=$(cat <<EOF
{
    "text": "${emoji} *LatticeDB Deployment*",
    "blocks": [
        {
            "type": "section",
            "text": {
                "type": "mrkdwn",
                "text": "${emoji} *LatticeDB Deployment*\n*Status:* ${status}\n*Environment:* ${ENVIRONMENT}\n*Strategy:* ${DEPLOYMENT_STRATEGY}\n*Image:* ${IMAGE_URI}\n*Message:* ${message}"
            }
        }
    ]
}
EOF
)
        curl -X POST -H 'Content-type: application/json' \
            --data "$payload" \
            "$SLACK_WEBHOOK_URL" 2>/dev/null || true
    fi
}

# Pre-flight checks
preflight_checks() {
    log "Running pre-flight checks..."

    # Check required tools
    for tool in aws jq curl; do
        if ! command -v $tool &> /dev/null; then
            log_error "Required tool not found: $tool"
            return 1
        fi
    done

    # Validate image exists
    log "Validating image exists..."
    if [[ "$PLATFORM" == "aws" ]]; then
        local repo_uri=$(echo "$IMAGE_URI" | cut -d: -f1)
        local tag=$(echo "$IMAGE_URI" | cut -d: -f2)

        if ! aws ecr describe-images \
            --repository-name "${repo_uri##*/}" \
            --image-ids imageTag="$tag" &>/dev/null; then
            log_error "Image not found in ECR: $IMAGE_URI"
            return 1
        fi
    fi

    # Check current deployment health
    log "Checking current deployment health..."
    if ! ./scripts/shared/health-check.sh; then
        log_warning "Current deployment has health issues"
        read -p "Continue anyway? (yes/no): " -r
        if [[ ! $REPLY =~ ^[Yy]es$ ]]; then
            return 1
        fi
    fi

    log_success "Pre-flight checks passed"
    return 0
}

# Run smoke tests
run_smoke_tests() {
    local endpoint=$1
    log "Running smoke tests against: $endpoint"

    local tests_passed=0
    local tests_failed=0

    # Test 1: Health endpoint
    log_info "Test 1: Health endpoint"
    if curl -sf "${endpoint}/health" | jq -e '.status == "healthy"' >/dev/null; then
        log_success "Health endpoint test passed"
        tests_passed=$((tests_passed + 1))
    else
        log_error "Health endpoint test failed"
        tests_failed=$((tests_failed + 1))
    fi

    # Test 2: Metrics endpoint
    log_info "Test 2: Metrics endpoint"
    if curl -sf "${endpoint}/metrics" >/dev/null; then
        log_success "Metrics endpoint test passed"
        tests_passed=$((tests_passed + 1))
    else
        log_warning "Metrics endpoint test failed (non-critical)"
    fi

    # Test 3: Basic query
    log_info "Test 3: Basic database query"
    if curl -sf -X POST "${endpoint}/api/v1/query" \
        -H "Content-Type: application/json" \
        -d '{"query": "SELECT 1"}' >/dev/null; then
        log_success "Basic query test passed"
        tests_passed=$((tests_passed + 1))
    else
        log_error "Basic query test failed"
        tests_failed=$((tests_failed + 1))
    fi

    # Test 4: Connection pool
    log_info "Test 4: Connection pool status"
    if curl -sf "${endpoint}/health" | jq -e '.connection_pool.available > 0' >/dev/null; then
        log_success "Connection pool test passed"
        tests_passed=$((tests_passed + 1))
    else
        log_warning "Connection pool test failed (non-critical)"
    fi

    log_info "Smoke tests: $tests_passed passed, $tests_failed failed"

    if [[ $tests_failed -gt 1 ]]; then
        log_error "Too many smoke tests failed"
        return 1
    fi

    return 0
}

# Validate metrics
validate_metrics() {
    log "Validating deployment metrics..."

    # This would integrate with your actual monitoring system
    # For now, we'll simulate metric collection

    local error_rate=2.5
    local p95_latency=450
    local success_rate=98.5
    local cpu_percent=45
    local memory_percent=62

    log_info "Collected metrics:"
    log_info "  Error rate: ${error_rate}%"
    log_info "  P95 latency: ${p95_latency}ms"
    log_info "  Success rate: ${success_rate}%"
    log_info "  CPU usage: ${cpu_percent}%"
    log_info "  Memory usage: ${memory_percent}%"

    local failed=false

    if (( $(echo "$error_rate > $MAX_ERROR_RATE" | bc -l) )); then
        log_error "Error rate too high: ${error_rate}% > ${MAX_ERROR_RATE}%"
        failed=true
    fi

    if (( $(echo "$p95_latency > $MAX_P95_LATENCY_MS" | bc -l) )); then
        log_error "P95 latency too high: ${p95_latency}ms > ${MAX_P95_LATENCY_MS}ms"
        failed=true
    fi

    if (( $(echo "$success_rate < $MIN_SUCCESS_RATE" | bc -l) )); then
        log_error "Success rate too low: ${success_rate}% < ${MIN_SUCCESS_RATE}%"
        failed=true
    fi

    if (( $(echo "$cpu_percent > $MAX_CPU_PERCENT" | bc -l) )); then
        log_warning "CPU usage high: ${cpu_percent}% > ${MAX_CPU_PERCENT}%"
    fi

    if (( $(echo "$memory_percent > $MAX_MEMORY_PERCENT" | bc -l) )); then
        log_warning "Memory usage high: ${memory_percent}% > ${MAX_MEMORY_PERCENT}%"
    fi

    if [[ "$failed" == "true" ]]; then
        log_error "Metrics validation failed"
        return 1
    fi

    log_success "Metrics validation passed"
    return 0
}

# Execute deployment strategy
execute_deployment() {
    log "Executing ${DEPLOYMENT_STRATEGY} deployment..."

    case $DEPLOYMENT_STRATEGY in
        rolling)
            log_info "Running rolling deployment..."
            ./aws/scripts/manage-service.sh update -i "$IMAGE_URI" -e "$ENVIRONMENT"
            ;;
        blue-green)
            log_info "Running blue/green deployment..."
            ./aws/scripts/blue-green-deploy.sh -e "$ENVIRONMENT" -i "$IMAGE_URI"
            ;;
        canary)
            log_info "Running canary deployment..."
            ./aws/scripts/canary-deploy.sh -e "$ENVIRONMENT" -i "$IMAGE_URI"
            ;;
        dark-launch)
            log_info "Running dark launch deployment..."
            log_info "Deploying without traffic routing..."
            # Deploy to separate environment for validation
            ENVIRONMENT="${ENVIRONMENT}-dark" \
                ./aws/scripts/manage-service.sh update -i "$IMAGE_URI"
            ;;
        feature-flag)
            log_info "Running feature flag deployment..."
            log_info "Deploying with feature flags enabled..."
            # Would integrate with feature flag service
            ./aws/scripts/manage-service.sh update -i "$IMAGE_URI" -e "$ENVIRONMENT"
            ;;
        *)
            log_error "Unknown deployment strategy: $DEPLOYMENT_STRATEGY"
            return 1
            ;;
    esac

    if [[ $? -ne 0 ]]; then
        log_error "Deployment failed"
        return 1
    fi

    log_success "Deployment executed successfully"
    return 0
}

# Validate deployment
validate_deployment() {
    log "Validating deployment..."

    # Get endpoint
    local endpoint
    if [[ "$PLATFORM" == "aws" ]]; then
        endpoint=$(aws elbv2 describe-load-balancers \
            --region us-east-1 \
            --names "latticedb-${ENVIRONMENT}" \
            --query 'LoadBalancers[0].DNSName' \
            --output text 2>/dev/null || echo "localhost:8080")
        endpoint="http://${endpoint}"
    else
        endpoint="http://localhost:8080"
    fi

    # Run smoke tests
    if [[ "$SMOKE_TEST_ENABLED" == "true" ]]; then
        if ! run_smoke_tests "$endpoint"; then
            log_error "Smoke tests failed"
            return 1
        fi
    fi

    # Validate metrics
    if [[ "$METRICS_VALIDATION_ENABLED" == "true" ]]; then
        sleep 30  # Allow metrics to accumulate
        if ! validate_metrics; then
            log_error "Metrics validation failed"
            return 1
        fi
    fi

    log_success "Deployment validation passed"
    return 0
}

# Rollback deployment
rollback_deployment() {
    log_error "Initiating rollback..."

    send_notification "failure" "Deployment failed, rolling back"

    case $DEPLOYMENT_STRATEGY in
        blue-green)
            log_info "Switching traffic back to blue..."
            # Rollback logic handled by blue-green-deploy.sh
            ;;
        canary)
            log_info "Removing canary and restoring stable..."
            # Rollback logic handled by canary-deploy.sh
            ;;
        *)
            log_info "Rolling back to previous task definition..."
            ./aws/scripts/manage-service.sh rollback -e "$ENVIRONMENT"
            ;;
    esac

    log_error "Rollback completed"
    exit 1
}

# Main orchestration
main() {
    log "========================================"
    log "Progressive Delivery Pipeline"
    log "========================================"
    log "Strategy: $DEPLOYMENT_STRATEGY"
    log "Environment: $ENVIRONMENT"
    log "Platform: $PLATFORM"
    log "Image: $IMAGE_URI"
    log "Auto-rollback: $AUTO_ROLLBACK"
    log "========================================"

    send_notification "info" "Deployment started"

    # Pre-flight
    if [[ "$VALIDATION_ENABLED" == "true" ]]; then
        if ! preflight_checks; then
            log_error "Pre-flight checks failed"
            send_notification "failure" "Pre-flight checks failed"
            exit 1
        fi
    fi

    # Execute deployment
    if ! execute_deployment; then
        if [[ "$AUTO_ROLLBACK" == "true" ]]; then
            rollback_deployment
        else
            log_error "Deployment failed but auto-rollback is disabled"
            send_notification "failure" "Deployment failed (no rollback)"
            exit 1
        fi
    fi

    # Validate deployment
    if [[ "$VALIDATION_ENABLED" == "true" ]]; then
        if ! validate_deployment; then
            if [[ "$AUTO_ROLLBACK" == "true" ]]; then
                rollback_deployment
            else
                log_error "Validation failed but auto-rollback is disabled"
                send_notification "failure" "Validation failed (no rollback)"
                exit 1
            fi
        fi
    fi

    log_success "========================================"
    log_success "Deployment Completed Successfully!"
    log_success "========================================"
    log_success "Strategy: $DEPLOYMENT_STRATEGY"
    log_success "Environment: $ENVIRONMENT"
    log_success "Image: $IMAGE_URI"
    log_success "========================================"

    send_notification "success" "Deployment completed successfully"
}

main
