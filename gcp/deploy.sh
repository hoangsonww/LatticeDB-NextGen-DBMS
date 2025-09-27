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
GCP_PROJECT="${GCP_PROJECT:-}"
GCP_REGION="${GCP_REGION:-us-central1}"
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

    if ! command -v gcloud &> /dev/null; then
        missing_deps+=("gcloud")
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
        echo "  Google Cloud SDK: https://cloud.google.com/sdk/docs/install"
        echo "  Terraform: https://learn.hashicorp.com/tutorials/terraform/install-cli"
        echo "  Docker: https://docs.docker.com/get-docker/"
        exit 1
    fi

    log_success "All dependencies are installed"
}

check_gcp_auth() {
    log "Checking GCP authentication..."

    if ! gcloud auth list --filter=status:ACTIVE --format="value(account)" &> /dev/null; then
        log_error "Not authenticated with GCP"
        echo ""
        echo "Please authenticate with GCP using one of these methods:"
        echo "  gcloud auth login"
        echo "  gcloud auth application-default login"
        exit 1
    fi

    local current_project=$(gcloud config get-value project 2>/dev/null || echo "")
    if [ -z "$current_project" ] && [ -z "$GCP_PROJECT" ]; then
        log_error "No GCP project set"
        echo ""
        echo "Please set a GCP project:"
        echo "  gcloud config set project YOUR_PROJECT_ID"
        echo "  export GCP_PROJECT=YOUR_PROJECT_ID"
        exit 1
    fi

    if [ -n "$GCP_PROJECT" ]; then
        gcloud config set project "$GCP_PROJECT"
        current_project="$GCP_PROJECT"
    fi

    local account=$(gcloud auth list --filter=status:ACTIVE --format="value(account)" | head -1)
    log_success "GCP authentication valid - Project: $current_project, Account: $account"
}

enable_apis() {
    log "Enabling required GCP APIs..."

    local apis=(
        "run.googleapis.com"
        "cloudbuild.googleapis.com"
        "artifactregistry.googleapis.com"
        "cloudresourcemanager.googleapis.com"
        "iam.googleapis.com"
        "monitoring.googleapis.com"
        "logging.googleapis.com"
        "storage.googleapis.com"
        "secretmanager.googleapis.com"
        "compute.googleapis.com"
        "vpcaccess.googleapis.com"
    )

    for api in "${apis[@]}"; do
        if ! gcloud services list --enabled --filter="name:$api" --format="value(name)" | grep -q "$api"; then
            log "Enabling $api..."
            gcloud services enable "$api"
        fi
    done

    log_success "Required APIs are enabled"
}

build_and_push_image() {
    log "Building and pushing Docker image..."

    # Get Artifact Registry repository URL
    local registry_url=$(terraform output -raw artifact_registry_url 2>/dev/null || echo "")

    if [ -z "$registry_url" ]; then
        log_error "Artifact Registry URL not found. Please run terraform apply first."
        exit 1
    fi

    # Configure Docker to use gcloud as a credential helper
    log "Configuring Docker authentication..."
    gcloud auth configure-docker "${GCP_REGION}-docker.pkg.dev" --quiet

    # Build image
    log "Building Docker image..."
    cd "$PROJECT_ROOT"
    docker build -t "${registry_url}/latticedb:${IMAGE_TAG}" .

    # Push image
    log "Pushing Docker image to Artifact Registry..."
    docker push "${registry_url}/latticedb:${IMAGE_TAG}"

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

    # Set project_id from gcloud if not provided
    local current_project=$(gcloud config get-value project 2>/dev/null)
    local tf_vars=(-var="project_id=$current_project" -var="gcp_region=$GCP_REGION" -var="environment=$ENVIRONMENT" -var="image_tag=$IMAGE_TAG")

    # Plan deployment
    log "Planning Terraform deployment..."
    terraform plan "${tf_vars[@]}"

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
    terraform apply -auto-approve "${tf_vars[@]}"

    log_success "Infrastructure deployed successfully"
}

update_cloud_run() {
    log "Updating Cloud Run service..."

    cd "$SCRIPT_DIR"

    local project_id=$(terraform output -raw project_id)
    local service_name=$(terraform output -raw cloud_run_service_name)
    local registry_url=$(terraform output -raw artifact_registry_url)

    # Update Cloud Run service with new image
    gcloud run services update "$service_name" \
        --image="${registry_url}/latticedb:${IMAGE_TAG}" \
        --region="$GCP_REGION" \
        --project="$project_id" \
        --platform=managed

    log_success "Cloud Run service updated successfully"
}

wait_for_deployment() {
    log "Waiting for deployment to complete..."

    cd "$SCRIPT_DIR"

    local project_id=$(terraform output -raw project_id)
    local service_name=$(terraform output -raw cloud_run_service_name)

    # Wait for the service to be ready
    local max_attempts=30
    local attempt=1

    while [ $attempt -le $max_attempts ]; do
        local status=$(gcloud run services describe "$service_name" \
            --region="$GCP_REGION" \
            --project="$project_id" \
            --format="value(status.conditions[0].status)" 2>/dev/null || echo "False")

        if [ "$status" = "True" ]; then
            break
        fi

        log "Waiting for Cloud Run service to be ready... (attempt $attempt/$max_attempts)"
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
    local project_id=$(terraform output -raw project_id)
    local service_name=$(terraform output -raw cloud_run_service_name)
    local custom_domain=$(terraform output -raw custom_domain)

    echo "🎉 LatticeDB has been deployed successfully!"
    echo ""
    echo "📡 Connection Information:"
    echo "   Application URL:  $app_url"
    echo "   Health Check:     $health_check_url"
    if [ -n "$custom_domain" ]; then
        echo "   Custom Domain:    https://$custom_domain"
    fi
    echo ""
    echo "📊 Monitoring:"
    echo "   Project ID:       $project_id"
    echo "   Cloud Run Service: $service_name"
    echo "   Region:           $GCP_REGION"
    echo ""
    echo "🔧 Management Commands:"
    echo "   View logs: gcloud logging read 'resource.type=cloud_run_revision resource.labels.service_name=$service_name' --limit=50 --project=$project_id"
    echo "   Scale service: gcloud run services update $service_name --min-instances=<min> --max-instances=<max> --region=$GCP_REGION --project=$project_id"
    echo "   Service status: gcloud run services describe $service_name --region=$GCP_REGION --project=$project_id"
    echo ""
    echo "💰 Cost Management:"
    echo "   View costs: gcloud billing budgets list --billing-account=\$(gcloud billing accounts list --format='value(name)' | head -1)"
    echo ""
    echo "🔒 Security:"
    echo "   View IAM policies: gcloud projects get-iam-policy $project_id"
    echo "   Service account: $(terraform output -raw service_account_email)"
    echo ""
}

cleanup() {
    log "Cleaning up resources..."

    cd "$SCRIPT_DIR"

    echo ""
    log_warning "This will destroy all GCP resources created by Terraform."
    read -p "Are you sure you want to continue? (y/N): " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_warning "Cleanup cancelled by user"
        exit 0
    fi

    local current_project=$(gcloud config get-value project 2>/dev/null)
    local tf_vars=(-var="project_id=$current_project" -var="gcp_region=$GCP_REGION" -var="environment=$ENVIRONMENT")

    terraform destroy -auto-approve "${tf_vars[@]}"

    log_success "Resources cleaned up successfully"
}

show_usage() {
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  deploy     - Deploy LatticeDB to GCP (default)"
    echo "  update     - Update the application without changing infrastructure"
    echo "  cleanup    - Destroy all GCP resources"
    echo "  help       - Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  GCP_PROJECT    - GCP project ID (required if not set in gcloud config)"
    echo "  GCP_REGION     - GCP region (default: us-central1)"
    echo "  ENVIRONMENT    - Deployment environment (default: production)"
    echo "  IMAGE_TAG      - Docker image tag (default: latest)"
    echo ""
    echo "Examples:"
    echo "  $0 deploy"
    echo "  GCP_PROJECT=my-project GCP_REGION=us-east1 ENVIRONMENT=staging $0 deploy"
    echo "  IMAGE_TAG=v1.0.0 $0 update"
}

main() {
    local command="${1:-deploy}"

    case "$command" in
        deploy)
            check_dependencies
            check_gcp_auth
            enable_apis
            deploy_infrastructure
            build_and_push_image
            update_cloud_run
            wait_for_deployment
            show_connection_info
            ;;
        update)
            check_dependencies
            check_gcp_auth
            build_and_push_image
            update_cloud_run
            wait_for_deployment
            show_connection_info
            ;;
        cleanup)
            check_dependencies
            check_gcp_auth
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