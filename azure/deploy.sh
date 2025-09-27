#!/bin/bash

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
AZURE_LOCATION="${AZURE_LOCATION:-East US}"
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

    if ! command -v az &> /dev/null; then
        missing_deps+=("azure-cli")
    fi

    if ! command -v terraform &> /dev/null; then
        missing_deps+=("terraform")
    fi

    if ! command -v docker &> /dev/null; then
        missing_deps+=("docker")
    fi

    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing_deps[*]}"
        echo ""
        echo "Please install the missing dependencies:"
        echo "  Azure CLI: https://docs.microsoft.com/en-us/cli/azure/install-azure-cli"
        echo "  Terraform: https://learn.hashicorp.com/tutorials/terraform/install-cli"
        echo "  Docker: https://docs.docker.com/get-docker/"
        exit 1
    fi

    log_success "All dependencies are installed"
}

check_azure_login() {
    log "Checking Azure authentication..."

    if ! az account show &> /dev/null; then
        log_error "Not logged in to Azure"
        echo ""
        echo "Please log in to Azure using one of these methods:"
        echo "  az login"
        echo "  az login --service-principal"
        exit 1
    fi

    local account_name=$(az account show --query name --output tsv)
    local subscription_id=$(az account show --query id --output tsv)
    log_success "Azure authentication valid - Account: $account_name, Subscription: $subscription_id"
}

build_and_push_image() {
    log "Building and pushing Docker image..."

    # Get ACR login server
    local acr_login_server=$(terraform output -raw container_registry_login_server 2>/dev/null || echo "")

    if [ -z "$acr_login_server" ]; then
        log_error "ACR login server not found. Please run terraform apply first."
        exit 1
    fi

    # Login to ACR
    log "Logging in to Azure Container Registry..."
    az acr login --name $(terraform output -raw container_registry_name)

    # Build image
    log "Building Docker image..."
    cd "$PROJECT_ROOT"
    docker build -t "${acr_login_server}/latticedb:${IMAGE_TAG}" .

    # Push image
    log "Pushing Docker image to ACR..."
    docker push "${acr_login_server}/latticedb:${IMAGE_TAG}"

    log_success "Docker image built and pushed successfully"
}

deploy_infrastructure() {
    log "Deploying infrastructure with Terraform..."

    cd "$SCRIPT_DIR"

    # Initialize Terraform
    log "Initializing Terraform..."
    terraform init

    # Validate configuration
    log "Validating Terraform configuration..."
    terraform validate

    # Plan deployment
    log "Planning Terraform deployment..."
    terraform plan -var="azure_location=$AZURE_LOCATION" -var="environment=$ENVIRONMENT" -var="image_tag=$IMAGE_TAG"

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
    terraform apply -auto-approve -var="azure_location=$AZURE_LOCATION" -var="environment=$ENVIRONMENT" -var="image_tag=$IMAGE_TAG"

    log_success "Infrastructure deployed successfully"
}

update_container_app() {
    log "Updating Container App..."

    cd "$SCRIPT_DIR"

    local resource_group=$(terraform output -raw resource_group_name)
    local container_app_name=$(terraform output -raw container_app_name)
    local acr_login_server=$(terraform output -raw container_registry_login_server)

    # Update container app with new image
    az containerapp update \
        --name "$container_app_name" \
        --resource-group "$resource_group" \
        --image "${acr_login_server}/latticedb:${IMAGE_TAG}" \
        --output table

    log_success "Container App updated successfully"
}

wait_for_deployment() {
    log "Waiting for deployment to complete..."

    cd "$SCRIPT_DIR"

    local resource_group=$(terraform output -raw resource_group_name)
    local container_app_name=$(terraform output -raw container_app_name)

    # Wait for the container app to be ready
    local max_attempts=30
    local attempt=1

    while [ $attempt -le $max_attempts ]; do
        local status=$(az containerapp show \
            --name "$container_app_name" \
            --resource-group "$resource_group" \
            --query "properties.provisioningState" \
            --output tsv)

        if [ "$status" = "Succeeded" ]; then
            break
        fi

        log "Waiting for container app to be ready... (attempt $attempt/$max_attempts)"
        sleep 10
        ((attempt++))
    done

    if [ $attempt -gt $max_attempts ]; then
        log_error "Timeout waiting for deployment to complete"
        exit 1
    fi

    log_success "Deployment completed successfully"
}

show_connection_info() {
    log "Deployment Summary:"
    echo ""

    cd "$SCRIPT_DIR"

    local app_url=$(terraform output -raw application_url)
    local health_check_url=$(terraform output -raw health_check_url)
    local resource_group=$(terraform output -raw resource_group_name)
    local container_app_name=$(terraform output -raw container_app_name)

    echo "🎉 LatticeDB has been deployed successfully!"
    echo ""
    echo "📡 Connection Information:"
    echo "   Application URL:  $app_url"
    echo "   Health Check:     $health_check_url"
    echo ""
    echo "📊 Monitoring:"
    echo "   Resource Group:   $resource_group"
    echo "   Container App:    $container_app_name"
    echo "   Log Analytics:    $(terraform output -raw log_analytics_workspace_name)"
    echo ""
    echo "🔧 Management Commands:"
    echo "   View logs: az containerapp logs show --name $container_app_name --resource-group $resource_group --follow"
    echo "   Scale app: az containerapp update --name $container_app_name --resource-group $resource_group --min-replicas <min> --max-replicas <max>"
    echo "   App status: az containerapp show --name $container_app_name --resource-group $resource_group"
    echo ""
    echo "💰 Cost Management:"
    echo "   View costs: az consumption usage list --start-date $(date -d '1 month ago' +%Y-%m-%d) --end-date $(date +%Y-%m-%d)"
    echo ""
}

cleanup() {
    log "Cleaning up resources..."

    cd "$SCRIPT_DIR"

    echo ""
    log_warning "This will destroy all Azure resources created by Terraform."
    read -p "Are you sure you want to continue? (y/N): " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_warning "Cleanup cancelled by user"
        exit 0
    fi

    terraform destroy -auto-approve -var="azure_location=$AZURE_LOCATION" -var="environment=$ENVIRONMENT"

    log_success "Resources cleaned up successfully"
}

show_usage() {
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  deploy     - Deploy LatticeDB to Azure (default)"
    echo "  update     - Update the application without changing infrastructure"
    echo "  cleanup    - Destroy all Azure resources"
    echo "  help       - Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  AZURE_LOCATION - Azure location (default: East US)"
    echo "  ENVIRONMENT    - Deployment environment (default: production)"
    echo "  IMAGE_TAG      - Docker image tag (default: latest)"
    echo ""
    echo "Examples:"
    echo "  $0 deploy"
    echo "  AZURE_LOCATION='West US 2' ENVIRONMENT=staging $0 deploy"
    echo "  IMAGE_TAG=v1.0.0 $0 update"
}

main() {
    local command="${1:-deploy}"

    case "$command" in
        deploy)
            check_dependencies
            check_azure_login
            deploy_infrastructure
            build_and_push_image
            update_container_app
            wait_for_deployment
            show_connection_info
            ;;
        update)
            check_dependencies
            check_azure_login
            build_and_push_image
            update_container_app
            wait_for_deployment
            show_connection_info
            ;;
        cleanup)
            check_dependencies
            check_azure_login
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

# Handle script interruption
trap 'log_error "Script interrupted"; exit 1' INT TERM

# Run main function
main "$@"