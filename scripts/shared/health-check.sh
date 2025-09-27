#!/bin/bash

# Shared health check script for all deployments
SERVICE_URL="${1:-http://localhost:8080}"
MAX_RETRIES="${2:-30}"
RETRY_INTERVAL="${3:-10}"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

check_health() {
    local url="$1/health"
    local response=$(curl -s -w "%{http_code}" -o /dev/null "$url" 2>/dev/null)

    if [ "$response" = "200" ]; then
        return 0
    else
        return 1
    fi
}

wait_for_service() {
    local retries=0

    print_status "Waiting for service at $SERVICE_URL to be healthy..."

    while [ $retries -lt $MAX_RETRIES ]; do
        if check_health "$SERVICE_URL"; then
            print_success "Service is healthy!"
            return 0
        fi

        retries=$((retries + 1))
        print_status "Attempt $retries/$MAX_RETRIES failed, retrying in ${RETRY_INTERVAL}s..."
        sleep $RETRY_INTERVAL
    done

    print_error "Service failed to become healthy after $MAX_RETRIES attempts"
    return 1
}

show_service_info() {
    print_status "Service information:"
    curl -s "$SERVICE_URL/health" | jq . 2>/dev/null || echo "Health endpoint not available"

    print_status "Service metrics:"
    curl -s "$SERVICE_URL/metrics" 2>/dev/null || echo "Metrics endpoint not available"
}

case "${1:-check}" in
    wait) wait_for_service ;;
    info) show_service_info ;;
    *)
        if check_health "$SERVICE_URL"; then
            print_success "Service is healthy"
            exit 0
        else
            print_error "Service is not healthy"
            exit 1
        fi
        ;;
esac