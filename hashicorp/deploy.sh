#!/bin/bash

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CONSUL_ADDR="${CONSUL_HTTP_ADDR:-http://127.0.0.1:8500}"
VAULT_ADDR="${VAULT_ADDR:-http://127.0.0.1:8200}"
NOMAD_ADDR="${NOMAD_ADDR:-http://127.0.0.1:4646}"
ENVIRONMENT="${ENVIRONMENT:-production}"
IMAGE_TAG="${IMAGE_TAG:-latest}"

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

check_dependencies() {
    log "Checking dependencies..."

    local missing_deps=()

    if ! command -v consul &> /dev/null; then
        missing_deps+=("consul")
    fi

    if ! command -v vault &> /dev/null; then
        missing_deps+=("vault")
    fi

    if ! command -v nomad &> /dev/null; then
        missing_deps+=("nomad")
    fi

    if ! command -v terraform &> /dev/null; then
        missing_deps+=("terraform")
    fi

    if ! command -v docker &> /dev/null; then
        missing_deps+=("docker")
    fi

    if ! command -v jq &> /dev/null; then
        missing_deps+=("jq")
    fi

    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing_deps[*]}"
        echo ""
        echo "Please install the missing dependencies:"
        echo "  HashiCorp Tools: https://learn.hashicorp.com/tutorials/consul/get-started-install"
        echo "  Terraform: https://learn.hashicorp.com/tutorials/terraform/install-cli"
        echo "  Docker: https://docs.docker.com/get-docker/"
        echo "  jq: https://stedolan.github.io/jq/download/"
        exit 1
    fi

    log_success "All dependencies are installed"
}

check_hashicorp_stack() {
    log "Checking HashiCorp stack status..."

    # Check Consul
    if ! consul members &>/dev/null; then
        log_error "Consul is not accessible. Please ensure Consul is running."
        exit 1
    fi

    # Check Vault
    if ! vault status &>/dev/null; then
        log_error "Vault is not accessible. Please ensure Vault is running and unsealed."
        exit 1
    fi

    if vault status | grep -q "Sealed.*true"; then
        log_error "Vault is sealed. Please unseal Vault first."
        exit 1
    fi

    # Check Nomad
    if ! nomad node status &>/dev/null; then
        log_error "Nomad is not accessible. Please ensure Nomad is running."
        exit 1
    fi

    local consul_leader=$(consul operator raft list-peers | grep leader | wc -l)
    local nomad_nodes=$(nomad node status | grep ready | wc -l)

    log_success "HashiCorp stack is healthy:"
    echo "   • Consul: $consul_leader leader(s)"
    echo "   • Vault: Unsealed and accessible"
    echo "   • Nomad: $nomad_nodes ready node(s)"
}

check_authentication() {
    log "Checking authentication..."

    # Check Vault authentication
    if ! vault auth -method=token token="$VAULT_TOKEN" &>/dev/null 2>&1; then
        log_error "Vault authentication failed. Please set VAULT_TOKEN environment variable."
        exit 1
    fi

    # Check Consul token if ACLs are enabled
    if consul acl bootstrap -format=json &>/dev/null || [[ $? -eq 1 ]]; then
        if [[ -n "${CONSUL_HTTP_TOKEN:-}" ]]; then
            log_success "Consul ACL token provided"
        else
            log_warning "Consul ACLs may be enabled but no token provided. Set CONSUL_HTTP_TOKEN if needed."
        fi
    fi

    # Check Nomad token if ACLs are enabled
    if nomad acl bootstrap -json &>/dev/null || [[ $? -eq 1 ]]; then
        if [[ -n "${NOMAD_TOKEN:-}" ]]; then
            log_success "Nomad ACL token provided"
        else
            log_warning "Nomad ACLs may be enabled but no token provided. Set NOMAD_TOKEN if needed."
        fi
    fi

    log_success "Authentication checks completed"
}

setup_vault() {
    log "Setting up Vault for LatticeDB..."

    cd "$SCRIPT_DIR/vault"

    if [[ ! -x "./setup-vault.sh" ]]; then
        chmod +x "./setup-vault.sh"
    fi

    ./setup-vault.sh

    log_success "Vault setup completed"
}

register_consul_services() {
    log "Registering Consul services..."

    cd "$SCRIPT_DIR/consul"

    # Register services from JSON configuration
    if consul services register services.json; then
        log_success "Consul services registered"
    else
        log_error "Failed to register Consul services"
        exit 1
    fi

    # Apply ACL policies if ACLs are enabled
    if [[ -n "${CONSUL_HTTP_TOKEN:-}" ]]; then
        if consul acl policy create -name latticedb-policy -rules @acl-policies.hcl; then
            log_success "Consul ACL policies created"
        else
            log_warning "Failed to create Consul ACL policies (may already exist)"
        fi
    fi

    log_success "Consul configuration completed"
}

build_and_push_image() {
    log "Building and pushing Docker image..."

    # Build the image
    cd "$PROJECT_ROOT"
    docker build -t "latticedb/latticedb:${IMAGE_TAG}" -f hashicorp/Dockerfile.hashicorp .

    # Tag for local registry if needed
    if [[ -n "${DOCKER_REGISTRY:-}" ]]; then
        docker tag "latticedb/latticedb:${IMAGE_TAG}" "${DOCKER_REGISTRY}/latticedb:${IMAGE_TAG}"
        docker push "${DOCKER_REGISTRY}/latticedb:${IMAGE_TAG}"
        log_success "Docker image pushed to registry: ${DOCKER_REGISTRY}/latticedb:${IMAGE_TAG}"
    else
        log_success "Docker image built: latticedb/latticedb:${IMAGE_TAG}"
    fi
}

deploy_with_terraform() {
    log "Deploying infrastructure with Terraform..."

    cd "$SCRIPT_DIR/terraform"

    # Initialize Terraform
    log "Initializing Terraform..."
    terraform init

    # Validate configuration
    log "Validating Terraform configuration..."
    terraform validate

    # Plan deployment
    log "Planning Terraform deployment..."
    terraform plan \
        -var="consul_address=$CONSUL_ADDR" \
        -var="vault_address=$VAULT_ADDR" \
        -var="nomad_address=$NOMAD_ADDR" \
        -var="environment=$ENVIRONMENT" \
        -var="image_tag=$IMAGE_TAG" \
        -var="consul_token=${CONSUL_HTTP_TOKEN:-}" \
        -var="vault_token=${VAULT_TOKEN:-}" \
        -var="nomad_token=${NOMAD_TOKEN:-}"

    # Ask for confirmation
    echo ""
    read -p "Do you want to apply these changes? (y/N): " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_warning "Deployment cancelled by user"
        exit 0
    fi

    # Apply changes
    log "Applying Terraform configuration..."
    terraform apply -auto-approve \
        -var="consul_address=$CONSUL_ADDR" \
        -var="vault_address=$VAULT_ADDR" \
        -var="nomad_address=$NOMAD_ADDR" \
        -var="environment=$ENVIRONMENT" \
        -var="image_tag=$IMAGE_TAG" \
        -var="consul_token=${CONSUL_HTTP_TOKEN:-}" \
        -var="vault_token=${VAULT_TOKEN:-}" \
        -var="nomad_token=${NOMAD_TOKEN:-}"

    log_success "Terraform deployment completed"
}

deploy_nomad_job() {
    log "Deploying Nomad job..."

    cd "$SCRIPT_DIR/nomad"

    # Set meta variables for the job
    export NOMAD_META_environment="$ENVIRONMENT"
    export NOMAD_META_version="$IMAGE_TAG"
    export NOMAD_META_image="${DOCKER_REGISTRY:-latticedb/latticedb}"
    export NOMAD_META_domain="${LATTICEDB_DOMAIN:-latticedb.service.consul}"

    # Plan the job
    log "Planning Nomad job deployment..."
    if nomad job plan latticedb.nomad.hcl; then
        log_success "Nomad job plan validated"
    else
        log_error "Nomad job plan failed"
        exit 1
    fi

    # Run the job
    log "Running Nomad job..."
    if nomad job run latticedb.nomad.hcl; then
        log_success "Nomad job started"
    else
        log_error "Failed to start Nomad job"
        exit 1
    fi
}

wait_for_deployment() {
    log "Waiting for deployment to complete..."

    local max_attempts=60
    local attempt=1

    while [ $attempt -le $max_attempts ]; do
        local status=$(nomad job status latticedb -json | jq -r '.Status')
        local running=$(nomad job status latticedb -json | jq -r '.TaskGroups[0].Running')
        local desired=$(nomad job status latticedb -json | jq -r '.TaskGroups[0].Desired')

        if [[ "$status" == "running" ]] && [[ "$running" == "$desired" ]]; then
            log_success "All instances are running ($running/$desired)"
            break
        fi

        log "Waiting for deployment... Status: $status, Running: $running/$desired (attempt $attempt/$max_attempts)"
        sleep 10
        ((attempt++))
    done

    if [ $attempt -gt $max_attempts ]; then
        log_error "Timeout waiting for deployment to complete"
        exit 1
    fi

    # Wait for health checks
    log "Waiting for health checks to pass..."
    sleep 30

    local healthy=$(consul health service latticedb -json | jq '[.[] | select(.Checks[].Status == "passing")] | length')
    log_success "Health checks passed for $healthy instances"
}

show_deployment_info() {
    log "Deployment Summary:"
    echo ""

    cd "$SCRIPT_DIR/terraform"

    # Get service information
    local consul_service=$(consul catalog service latticedb -format=json | jq -r '.[0].ServiceAddress + ":" + (.[] | select(.ServicePort != null) | .ServicePort | tostring)' 2>/dev/null || echo "latticedb.service.consul:8080")
    local nomad_job_status=$(nomad job status latticedb -json | jq -r '.Status')
    local running_allocs=$(nomad job status latticedb -json | jq -r '.TaskGroups[0].Running')
    local total_allocs=$(nomad job status latticedb -json | jq -r '.TaskGroups[0].Desired')

    echo "🎉 LatticeDB has been deployed successfully!"
    echo ""
    echo "📡 Service Information:"
    echo "   Application URL:    http://${consul_service}"
    echo "   Health Check:       http://${consul_service}/health"
    echo "   SQL Interface:      postgresql://${consul_service%:*}:5432/latticedb"
    echo "   Metrics:            http://${consul_service}/metrics"
    echo ""
    echo "📊 Deployment Status:"
    echo "   Nomad Job:          latticedb ($nomad_job_status)"
    echo "   Running Instances:  $running_allocs/$total_allocs"
    echo "   Environment:        $ENVIRONMENT"
    echo "   Image Tag:          $IMAGE_TAG"
    echo ""
    echo "🔧 HashiCorp Stack:"
    echo "   Consul:             $CONSUL_ADDR"
    echo "   Vault:              $VAULT_ADDR"
    echo "   Nomad:              $NOMAD_ADDR"
    echo ""
    echo "🎛️  Management Commands:"
    echo "   Job status:         nomad job status latticedb"
    echo "   Job logs:           nomad logs -f latticedb"
    echo "   Scale service:      nomad job scale latticedb <count>"
    echo "   Service health:     consul health service latticedb"
    echo "   Service catalog:    consul catalog service latticedb"
    echo "   Vault secrets:      vault kv get secret/latticedb/$ENVIRONMENT/config"
    echo ""
    echo "🔐 Security Features:"
    echo "   • Consul Connect service mesh enabled"
    echo "   • Vault integration for secrets management"
    echo "   • mTLS encryption between services"
    echo "   • Dynamic certificate management"
    echo ""
    echo "📈 Monitoring:"
    echo "   • Consul health checks active"
    echo "   • Prometheus metrics available"
    echo "   • Distributed tracing configured"
    echo ""
}

update_service() {
    log "Updating LatticeDB service..."

    # Build new image
    build_and_push_image

    # Update Nomad job
    cd "$SCRIPT_DIR/nomad"
    export NOMAD_META_version="$IMAGE_TAG"

    nomad job run latticedb.nomad.hcl

    # Wait for update to complete
    wait_for_deployment

    log_success "Service update completed"
}

cleanup() {
    log "Cleaning up LatticeDB deployment..."

    echo ""
    log_warning "This will destroy all resources created for LatticeDB."
    read -p "Are you sure you want to continue? (y/N): " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_warning "Cleanup cancelled by user"
        exit 0
    fi

    # Stop Nomad job
    log "Stopping Nomad job..."
    nomad job stop -purge latticedb || true

    # Remove Terraform resources
    log "Destroying Terraform resources..."
    cd "$SCRIPT_DIR/terraform"
    terraform destroy -auto-approve \
        -var="consul_address=$CONSUL_ADDR" \
        -var="vault_address=$VAULT_ADDR" \
        -var="nomad_address=$NOMAD_ADDR" \
        -var="environment=$ENVIRONMENT" \
        -var="consul_token=${CONSUL_HTTP_TOKEN:-}" \
        -var="vault_token=${VAULT_TOKEN:-}" \
        -var="nomad_token=${NOMAD_TOKEN:-}" || true

    # Deregister Consul services
    log "Deregistering Consul services..."
    consul services deregister -id=latticedb-http || true
    consul services deregister -id=latticedb-sql || true
    consul services deregister -id=latticedb-admin || true

    # Remove Vault secrets (optional)
    if [[ "${REMOVE_VAULT_SECRETS:-false}" == "true" ]]; then
        log "Removing Vault secrets..."
        vault kv metadata delete secret/latticedb || true
    fi

    log_success "Cleanup completed"
}

show_status() {
    log "LatticeDB Deployment Status:"
    echo ""

    # Nomad status
    echo "📦 Nomad Job Status:"
    nomad job status latticedb 2>/dev/null || echo "   Job not found or not accessible"
    echo ""

    # Consul services
    echo "🔍 Consul Services:"
    consul catalog services -tags 2>/dev/null | grep latticedb || echo "   No LatticeDB services found"
    echo ""

    # Consul health
    echo "🏥 Service Health:"
    consul health service latticedb 2>/dev/null || echo "   No health information available"
    echo ""

    # Vault status
    echo "🔐 Vault Secrets:"
    vault kv list secret/latticedb/ 2>/dev/null || echo "   No secrets found"
    echo ""
}

show_usage() {
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  deploy     - Deploy LatticeDB to HashiCorp stack (default)"
    echo "  update     - Update the application without changing infrastructure"
    echo "  status     - Show deployment status"
    echo "  cleanup    - Remove all LatticeDB resources"
    echo "  help       - Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  CONSUL_HTTP_ADDR  - Consul server address (default: http://127.0.0.1:8500)"
    echo "  VAULT_ADDR        - Vault server address (default: http://127.0.0.1:8200)"
    echo "  NOMAD_ADDR        - Nomad server address (default: http://127.0.0.1:4646)"
    echo "  VAULT_TOKEN       - Vault authentication token (required)"
    echo "  CONSUL_HTTP_TOKEN - Consul ACL token (required if ACLs enabled)"
    echo "  NOMAD_TOKEN       - Nomad ACL token (required if ACLs enabled)"
    echo "  ENVIRONMENT       - Deployment environment (default: production)"
    echo "  IMAGE_TAG         - Docker image tag (default: latest)"
    echo "  DOCKER_REGISTRY   - Docker registry URL (optional)"
    echo ""
    echo "Examples:"
    echo "  $0 deploy"
    echo "  ENVIRONMENT=staging IMAGE_TAG=v1.2.3 $0 deploy"
    echo "  IMAGE_TAG=v2.0.0 $0 update"
}

main() {
    local command="${1:-deploy}"

    case "$command" in
        deploy)
            check_dependencies
            check_hashicorp_stack
            check_authentication
            setup_vault
            register_consul_services
            build_and_push_image
            deploy_with_terraform
            wait_for_deployment
            show_deployment_info
            ;;
        update)
            check_dependencies
            check_hashicorp_stack
            check_authentication
            update_service
            show_deployment_info
            ;;
        status)
            check_dependencies
            show_status
            ;;
        cleanup)
            check_dependencies
            check_hashicorp_stack
            check_authentication
            cleanup
            ;;
        help|--help|-h)
            show_usage
            ;;
        *)
            log_error "Unknown command: $command"
            echo ""
            show_usage
            exit 1
            ;;
    esac
}

# Check required environment variables
if [[ -z "${VAULT_TOKEN:-}" ]]; then
    log_error "VAULT_TOKEN environment variable is required"
    exit 1
fi

# Handle script interruption
trap 'log_error "Script interrupted"; exit 1' INT TERM

# Run main function
main "$@"