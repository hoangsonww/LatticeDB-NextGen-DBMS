#!/bin/bash

set -euo pipefail

# Health check script for HashiCorp deployment monitoring

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
CONSUL_ADDR="${CONSUL_HTTP_ADDR:-http://127.0.0.1:8500}"
VAULT_ADDR="${VAULT_ADDR:-http://127.0.0.1:8200}"
NOMAD_ADDR="${NOMAD_ADDR:-http://127.0.0.1:4646}"

# Functions
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

check_consul_health() {
    log "Checking Consul health..."

    if ! curl -sf "$CONSUL_ADDR/v1/status/leader" >/dev/null 2>&1; then
        log_error "Consul is not accessible"
        return 1
    fi

    local leader=$(curl -s "$CONSUL_ADDR/v1/status/leader" | tr -d '"')
    local peers=$(curl -s "$CONSUL_ADDR/v1/status/peers" | jq length)

    log_success "Consul is healthy - Leader: $leader, Peers: $peers"

    # Check LatticeDB service registration
    local services=$(curl -s "$CONSUL_ADDR/v1/catalog/service/latticedb" | jq length)
    if [[ $services -gt 0 ]]; then
        log_success "LatticeDB services registered: $services"
    else
        log_warning "No LatticeDB services found in Consul"
    fi

    # Check service health
    local healthy_services=$(curl -s "$CONSUL_ADDR/v1/health/service/latticedb?passing=true" | jq length)
    log "Healthy LatticeDB instances: $healthy_services/$services"
}

check_vault_health() {
    log "Checking Vault health..."

    local vault_status=$(curl -s "$VAULT_ADDR/v1/sys/health" 2>/dev/null || echo '{}')
    local initialized=$(echo "$vault_status" | jq -r '.initialized // false')
    local sealed=$(echo "$vault_status" | jq -r '.sealed // true')

    if [[ "$initialized" == "true" ]] && [[ "$sealed" == "false" ]]; then
        log_success "Vault is healthy - Initialized: $initialized, Sealed: $sealed"
    else
        log_error "Vault is not healthy - Initialized: $initialized, Sealed: $sealed"
        return 1
    fi

    # Check if we can authenticate (requires VAULT_TOKEN)
    if [[ -n "${VAULT_TOKEN:-}" ]]; then
        if vault token lookup >/dev/null 2>&1; then
            log_success "Vault authentication successful"

            # Check LatticeDB secrets
            local secrets=$(vault kv list -format=json secret/latticedb/ 2>/dev/null | jq -r '.[]' | wc -l)
            log "LatticeDB secret environments: $secrets"
        else
            log_warning "Vault authentication failed"
        fi
    else
        log_warning "VAULT_TOKEN not provided, skipping authentication check"
    fi
}

check_nomad_health() {
    log "Checking Nomad health..."

    local leader_info=$(nomad server members -json 2>/dev/null | jq -r '.[] | select(.Status == "alive" and .Tags.bootstrap == "1") | .Name' | head -1)
    if [[ -n "$leader_info" ]]; then
        log_success "Nomad is healthy - Leader: $leader_info"
    else
        log_error "Nomad leader not found"
        return 1
    fi

    # Check node status
    local ready_nodes=$(nomad node status -json 2>/dev/null | jq '[.[] | select(.Status == "ready")] | length')
    local total_nodes=$(nomad node status -json 2>/dev/null | jq 'length')
    log "Nomad nodes ready: $ready_nodes/$total_nodes"

    # Check LatticeDB job status
    local job_status=$(nomad job status latticedb -json 2>/dev/null | jq -r '.Status // "not-found"')
    if [[ "$job_status" == "running" ]]; then
        local running_allocs=$(nomad job status latticedb -json | jq -r '.TaskGroups[0].Running // 0')
        local desired_allocs=$(nomad job status latticedb -json | jq -r '.TaskGroups[0].Desired // 0')
        log_success "LatticeDB job is running - Allocations: $running_allocs/$desired_allocs"
    else
        log_warning "LatticeDB job status: $job_status"
    fi
}

check_latticedb_health() {
    log "Checking LatticeDB application health..."

    # Get service endpoints from Consul
    local endpoints=$(curl -s "$CONSUL_ADDR/v1/catalog/service/latticedb" | jq -r '.[] | "\(.ServiceAddress):\(.ServicePort)"')

    local healthy_count=0
    local total_count=0

    while read -r endpoint; do
        if [[ -n "$endpoint" ]] && [[ "$endpoint" != "null:null" ]]; then
            total_count=$((total_count + 1))

            if curl -sf "http://$endpoint/health" >/dev/null 2>&1; then
                healthy_count=$((healthy_count + 1))
                log_success "LatticeDB instance healthy: $endpoint"
            else
                log_error "LatticeDB instance unhealthy: $endpoint"
            fi
        fi
    done <<< "$endpoints"

    if [[ $total_count -gt 0 ]]; then
        log "LatticeDB health summary: $healthy_count/$total_count instances healthy"

        if [[ $healthy_count -eq $total_count ]]; then
            return 0
        else
            return 1
        fi
    else
        log_warning "No LatticeDB instances found"
        return 1
    fi
}

check_connectivity() {
    log "Checking service connectivity..."

    # Test SQL connectivity if possible
    local sql_endpoint=$(curl -s "$CONSUL_ADDR/v1/catalog/service/latticedb-sql" | jq -r '.[0] | "\(.ServiceAddress):\(.ServicePort)"' 2>/dev/null)

    if [[ -n "$sql_endpoint" ]] && [[ "$sql_endpoint" != "null:null" ]]; then
        if timeout 5 bash -c "echo >/dev/tcp/${sql_endpoint/:/ }" 2>/dev/null; then
            log_success "SQL port accessible: $sql_endpoint"
        else
            log_warning "SQL port not accessible: $sql_endpoint"
        fi
    fi

    # Test service mesh connectivity
    local connect_services=$(curl -s "$CONSUL_ADDR/v1/catalog/services" | jq -r 'keys[]' | grep -c ".*-sidecar-proxy" || echo "0")
    log "Service mesh proxies: $connect_services"
}

check_monitoring() {
    log "Checking monitoring and metrics..."

    # Check if metrics endpoints are accessible
    local endpoints=$(curl -s "$CONSUL_ADDR/v1/catalog/service/latticedb" | jq -r '.[] | "\(.ServiceAddress):\(.ServicePort)"')

    while read -r endpoint; do
        if [[ -n "$endpoint" ]] && [[ "$endpoint" != "null:null" ]]; then
            if curl -sf "http://$endpoint/metrics" >/dev/null 2>&1; then
                log_success "Metrics endpoint accessible: $endpoint/metrics"
            else
                log_warning "Metrics endpoint not accessible: $endpoint/metrics"
            fi
        fi
    done <<< "$endpoints"
}

generate_report() {
    local exit_code=0

    echo ""
    log "=== LatticeDB Health Check Report ==="
    echo ""

    # Overall status
    if check_consul_health && check_vault_health && check_nomad_health; then
        log_success "HashiCorp stack is healthy"
    else
        log_error "HashiCorp stack has issues"
        exit_code=1
    fi

    if check_latticedb_health; then
        log_success "LatticeDB application is healthy"
    else
        log_error "LatticeDB application has issues"
        exit_code=1
    fi

    check_connectivity
    check_monitoring

    echo ""
    if [[ $exit_code -eq 0 ]]; then
        log_success "All health checks passed"
    else
        log_error "Some health checks failed"
    fi

    return $exit_code
}

# Main execution
main() {
    case "${1:-check}" in
        check)
            generate_report
            ;;
        consul)
            check_consul_health
            ;;
        vault)
            check_vault_health
            ;;
        nomad)
            check_nomad_health
            ;;
        app|latticedb)
            check_latticedb_health
            ;;
        connectivity)
            check_connectivity
            ;;
        monitoring)
            check_monitoring
            ;;
        *)
            echo "Usage: $0 [check|consul|vault|nomad|app|connectivity|monitoring]"
            exit 1
            ;;
    esac
}

main "$@"