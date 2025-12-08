#!/bin/bash

# Feature Flag Management Script
# Manage feature flags and A/B experiments

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
FLAG_CONFIG=${FLAG_CONFIG:-"./config/feature-flags/flags.yaml"}
REDIS_HOST=${REDIS_HOST:-"localhost"}
REDIS_PORT=${REDIS_PORT:-6379}
REDIS_DB=${REDIS_DB:-1}
KEY_PREFIX="latticedb:flags:"

usage() {
    cat <<EOF
Usage: $0 <COMMAND> [OPTIONS]

Feature Flag Management

COMMANDS:
    list                    List all feature flags
    show FLAG_NAME          Show details of a specific flag
    enable FLAG_NAME        Enable a feature flag
    disable FLAG_NAME       Disable a feature flag
    set-percentage FLAG PCT Set rollout percentage
    create-experiment NAME  Create a new A/B experiment
    stop-experiment NAME    Stop an experiment
    get-variant USER FLAG   Get variant assignment for user
    report FLAG             Generate flag usage report

OPTIONS:
    --environment ENV       Environment (dev/staging/prod)
    --force                 Force operation without confirmation
    -h, --help              Show this help

EXAMPLES:
    # List all flags
    $0 list

    # Enable a feature flag
    $0 enable new_query_optimizer

    # Set rollout percentage
    $0 set-percentage new_query_optimizer 25

    # Get user's variant
    $0 get-variant user123 query_execution_strategy

    # Generate report
    $0 report new_query_optimizer

EOF
    exit 1
}

# Logging
log() { echo -e "${BLUE}[FLAGS]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

# Redis commands
redis_get() {
    local key=$1
    redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" -n "$REDIS_DB" GET "${KEY_PREFIX}${key}" 2>/dev/null || echo ""
}

redis_set() {
    local key=$1
    local value=$2
    redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" -n "$REDIS_DB" SET "${KEY_PREFIX}${key}" "$value" >/dev/null
}

redis_delete() {
    local key=$1
    redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" -n "$REDIS_DB" DEL "${KEY_PREFIX}${key}" >/dev/null
}

redis_list_keys() {
    redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" -n "$REDIS_DB" KEYS "${KEY_PREFIX}*" 2>/dev/null | sed "s|^${KEY_PREFIX}||"
}

# List all feature flags
list_flags() {
    log "Listing all feature flags..."

    if ! command -v yq &> /dev/null; then
        log_error "yq not found. Please install yq to use this feature."
        log "Listing from Redis instead..."

        local keys=$(redis_list_keys)
        if [[ -z "$keys" ]]; then
            log_warning "No flags found in Redis"
            return
        fi

        echo ""
        printf "%-30s %-10s %-15s\n" "FLAG NAME" "ENABLED" "ROLLOUT %"
        printf "%-30s %-10s %-15s\n" "----------" "-------" "---------"

        while read -r key; do
            local value=$(redis_get "$key")
            local enabled=$(echo "$value" | jq -r '.enabled // false')
            local percentage=$(echo "$value" | jq -r '.rollout.percentage // "N/A"')

            printf "%-30s %-10s %-15s\n" "$key" "$enabled" "$percentage"
        done <<< "$keys"

    else
        echo ""
        printf "%-30s %-10s %-15s %-40s\n" "FLAG NAME" "ENABLED" "ROLLOUT %" "DESCRIPTION"
        printf "%-30s %-10s %-15s %-40s\n" "----------" "-------" "---------" "-----------"

        yq eval '.features[] | [.name, .enabled, .rollout.percentage // "N/A", .description] | @tsv' "$FLAG_CONFIG" | \
        while IFS=$'\t' read -r name enabled percentage description; do
            printf "%-30s %-10s %-15s %-40s\n" "$name" "$enabled" "$percentage" "${description:0:40}"
        done
    fi

    echo ""
}

# Show flag details
show_flag() {
    local flag_name=$1

    log "Showing details for flag: $flag_name"

    # Check Redis first
    local redis_value=$(redis_get "$flag_name")

    if [[ -n "$redis_value" ]]; then
        echo ""
        echo "=== Redis Configuration ==="
        echo "$redis_value" | jq .
    fi

    # Check YAML config
    if command -v yq &> /dev/null; then
        local yaml_config=$(yq eval ".features[] | select(.name == \"$flag_name\")" "$FLAG_CONFIG")

        if [[ -n "$yaml_config" ]]; then
            echo ""
            echo "=== YAML Configuration ==="
            echo "$yaml_config"
        fi
    fi

    echo ""
}

# Enable a feature flag
enable_flag() {
    local flag_name=$1
    local force=${2:-false}

    if [[ "$force" != "true" ]]; then
        read -p "Enable flag '$flag_name'? (yes/no): " -r
        if [[ ! $REPLY =~ ^[Yy]es$ ]]; then
            log "Aborted"
            return 1
        fi
    fi

    log "Enabling flag: $flag_name"

    # Get current config
    local current=$(redis_get "$flag_name")

    if [[ -z "$current" ]]; then
        # Create new config
        current=$(cat <<EOF
{
  "name": "$flag_name",
  "enabled": true,
  "rollout": {"percentage": 100},
  "updated_at": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "updated_by": "$USER"
}
EOF
)
    else
        # Update existing
        current=$(echo "$current" | jq '.enabled = true | .updated_at = "'$(date -u +"%Y-%m-%dT%H:%M:%SZ")'" | .updated_by = "'$USER'"')
    fi

    redis_set "$flag_name" "$current"

    log_success "Flag '$flag_name' enabled"

    # Audit log
    log "Recording audit event..."
    local audit_key="audit:$(date +%s):enable:$flag_name"
    redis_set "$audit_key" "{\"flag\":\"$flag_name\",\"action\":\"enable\",\"user\":\"$USER\",\"timestamp\":\"$(date -u +"%Y-%m-%dT%H:%M:%SZ")\"}"
}

# Disable a feature flag
disable_flag() {
    local flag_name=$1
    local force=${2:-false}

    if [[ "$force" != "true" ]]; then
        log_warning "This will disable '$flag_name' for all users!"
        read -p "Continue? (yes/no): " -r
        if [[ ! $REPLY =~ ^[Yy]es$ ]]; then
            log "Aborted"
            return 1
        fi
    fi

    log "Disabling flag: $flag_name"

    local current=$(redis_get "$flag_name")

    if [[ -z "$current" ]]; then
        log_error "Flag not found: $flag_name"
        return 1
    fi

    current=$(echo "$current" | jq '.enabled = false | .updated_at = "'$(date -u +"%Y-%m-%dT%H:%M:%SZ")'" | .updated_by = "'$USER'"')

    redis_set "$flag_name" "$current"

    log_success "Flag '$flag_name' disabled"

    # Audit log
    local audit_key="audit:$(date +%s):disable:$flag_name"
    redis_set "$audit_key" "{\"flag\":\"$flag_name\",\"action\":\"disable\",\"user\":\"$USER\",\"timestamp\":\"$(date -u +"%Y-%m-%dT%H:%M:%SZ")\"}"
}

# Set rollout percentage
set_percentage() {
    local flag_name=$1
    local percentage=$2

    if [[ ! "$percentage" =~ ^[0-9]+$ ]] || [[ "$percentage" -lt 0 ]] || [[ "$percentage" -gt 100 ]]; then
        log_error "Percentage must be between 0 and 100"
        return 1
    fi

    log "Setting rollout percentage for '$flag_name' to $percentage%"

    local current=$(redis_get "$flag_name")

    if [[ -z "$current" ]]; then
        log_error "Flag not found: $flag_name"
        return 1
    fi

    current=$(echo "$current" | jq --arg pct "$percentage" '.rollout.percentage = ($pct | tonumber) | .updated_at = "'$(date -u +"%Y-%m-%dT%H:%M:%SZ")'" | .updated_by = "'$USER'"')

    redis_set "$flag_name" "$current"

    log_success "Rollout percentage set to $percentage%"

    # Audit log
    local audit_key="audit:$(date +%s):set-percentage:$flag_name"
    redis_set "$audit_key" "{\"flag\":\"$flag_name\",\"action\":\"set_percentage\",\"percentage\":$percentage,\"user\":\"$USER\",\"timestamp\":\"$(date -u +"%Y-%m-%dT%H:%M:%SZ")\"}"
}

# Get variant assignment
get_variant() {
    local user_id=$1
    local flag_name=$2

    log "Getting variant for user '$user_id' on flag '$flag_name'"

    # Simple hash-based assignment
    local hash=$(echo -n "${user_id}${flag_name}" | md5sum | awk '{print $1}')
    local hash_num=$((16#${hash:0:8}))
    local bucket=$((hash_num % 100))

    local flag_config=$(redis_get "$flag_name")

    if [[ -z "$flag_config" ]]; then
        log_error "Flag not found: $flag_name"
        return 1
    fi

    local enabled=$(echo "$flag_config" | jq -r '.enabled')
    local percentage=$(echo "$flag_config" | jq -r '.rollout.percentage // 100')

    if [[ "$enabled" == "false" ]]; then
        echo "control (flag disabled)"
        return 0
    fi

    if [[ $bucket -lt $percentage ]]; then
        echo "treatment"
    else
        echo "control"
    fi
}

# Generate report
generate_report() {
    local flag_name=$1

    log "Generating report for flag: $flag_name"

    # In production, this would query metrics from CloudWatch/Prometheus
    echo ""
    echo "==================================================================="
    echo "Feature Flag Report: $flag_name"
    echo "==================================================================="
    echo "Generated: $(date)"
    echo ""

    show_flag "$flag_name"

    echo ""
    echo "=== Usage Metrics (Last 24h) ==="
    echo "Total Evaluations: (metrics not implemented)"
    echo "Unique Users: (metrics not implemented)"
    echo "Variant Distribution: (metrics not implemented)"
    echo ""
    echo "=== Performance Impact ==="
    echo "Error Rate: (metrics not implemented)"
    echo "Latency P95: (metrics not implemented)"
    echo ""
    echo "==================================================================="
}

# Parse command
COMMAND=${1:-}
shift || true

case $COMMAND in
    list)
        list_flags
        ;;
    show)
        FLAG_NAME=${1:-}
        if [[ -z "$FLAG_NAME" ]]; then
            log_error "Flag name required"
            usage
        fi
        show_flag "$FLAG_NAME"
        ;;
    enable)
        FLAG_NAME=${1:-}
        if [[ -z "$FLAG_NAME" ]]; then
            log_error "Flag name required"
            usage
        fi
        enable_flag "$FLAG_NAME" "${FORCE:-false}"
        ;;
    disable)
        FLAG_NAME=${1:-}
        if [[ -z "$FLAG_NAME" ]]; then
            log_error "Flag name required"
            usage
        fi
        disable_flag "$FLAG_NAME" "${FORCE:-false}"
        ;;
    set-percentage)
        FLAG_NAME=${1:-}
        PERCENTAGE=${2:-}
        if [[ -z "$FLAG_NAME" ]] || [[ -z "$PERCENTAGE" ]]; then
            log_error "Flag name and percentage required"
            usage
        fi
        set_percentage "$FLAG_NAME" "$PERCENTAGE"
        ;;
    get-variant)
        USER_ID=${1:-}
        FLAG_NAME=${2:-}
        if [[ -z "$USER_ID" ]] || [[ -z "$FLAG_NAME" ]]; then
            log_error "User ID and flag name required"
            usage
        fi
        get_variant "$USER_ID" "$FLAG_NAME"
        ;;
    report)
        FLAG_NAME=${1:-}
        if [[ -z "$FLAG_NAME" ]]; then
            log_error "Flag name required"
            usage
        fi
        generate_report "$FLAG_NAME"
        ;;
    *)
        log_error "Unknown command: $COMMAND"
        usage
        ;;
esac
