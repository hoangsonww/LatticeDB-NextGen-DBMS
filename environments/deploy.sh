#!/bin/bash
# Multi-environment deployment script for LatticeDB

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

# Usage function
usage() {
    echo "Usage: $0 -p <platform> -e <environment> [options]"
    echo ""
    echo "Platforms: aws, azure, gcp, hashicorp"
    echo "Environments: dev, staging, prod"
    echo ""
    echo "Options:"
    echo "  -p, --platform      Target platform (aws|azure|gcp|hashicorp)"
    echo "  -e, --environment   Target environment (dev|staging|prod)"
    echo "  -d, --destroy       Destroy infrastructure instead of deploying"
    echo "  -f, --force         Skip confirmation prompts"
    echo "  -v, --validate      Validate configuration only"
    echo "  -h, --help         Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 -p aws -e dev                    # Deploy to AWS dev environment"
    echo "  $0 -p azure -e prod --validate      # Validate Azure prod configuration"
    echo "  $0 -p gcp -e staging --destroy      # Destroy GCP staging environment"
    exit 1
}

# Logging functions
log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $*"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

# Validate prerequisites
validate_prerequisites() {
    local platform=$1

    log_info "Validating prerequisites for $platform..."

    case $platform in
        aws)
            command -v aws >/dev/null 2>&1 || { log_error "AWS CLI not found. Please install it."; exit 1; }
            command -v terraform >/dev/null 2>&1 || { log_error "Terraform not found. Please install it."; exit 1; }
            aws sts get-caller-identity >/dev/null || { log_error "AWS credentials not configured."; exit 1; }
            ;;
        azure)
            command -v az >/dev/null 2>&1 || { log_error "Azure CLI not found. Please install it."; exit 1; }
            command -v terraform >/dev/null 2>&1 || { log_error "Terraform not found. Please install it."; exit 1; }
            az account show >/dev/null || { log_error "Azure credentials not configured."; exit 1; }
            ;;
        gcp)
            command -v gcloud >/dev/null 2>&1 || { log_error "Google Cloud CLI not found. Please install it."; exit 1; }
            command -v terraform >/dev/null 2>&1 || { log_error "Terraform not found. Please install it."; exit 1; }
            gcloud auth list --filter=status:ACTIVE --format="value(account)" | grep -q . || { log_error "GCP credentials not configured."; exit 1; }
            ;;
        hashicorp)
            command -v terraform >/dev/null 2>&1 || { log_error "Terraform not found. Please install it."; exit 1; }
            command -v nomad >/dev/null 2>&1 || { log_error "Nomad CLI not found. Please install it."; exit 1; }
            command -v consul >/dev/null 2>&1 || { log_error "Consul CLI not found. Please install it."; exit 1; }
            command -v vault >/dev/null 2>&1 || { log_error "Vault CLI not found. Please install it."; exit 1; }
            ;;
    esac

    log_success "Prerequisites validated successfully"
}

# Validate configuration
validate_config() {
    local platform=$1
    local environment=$2
    local tfvars_file="$SCRIPT_DIR/$environment/$platform.tfvars"
    local terraform_dir="$PROJECT_ROOT/$platform"

    log_info "Validating configuration for $platform/$environment..."

    # Check if tfvars file exists
    if [[ ! -f "$tfvars_file" ]]; then
        log_error "Configuration file not found: $tfvars_file"
        exit 1
    fi

    # Check if terraform directory exists
    if [[ ! -d "$terraform_dir" ]]; then
        log_error "Terraform directory not found: $terraform_dir"
        exit 1
    fi

    # Validate terraform configuration
    cd "$terraform_dir"
    terraform init -backend=false >/dev/null 2>&1
    terraform validate || { log_error "Terraform validation failed"; exit 1; }

    log_success "Configuration validated successfully"
}

# Deploy infrastructure
deploy() {
    local platform=$1
    local environment=$2
    local tfvars_file="$SCRIPT_DIR/$environment/$platform.tfvars"
    local terraform_dir="$PROJECT_ROOT/$platform"

    log_info "Deploying $platform/$environment environment..."

    cd "$terraform_dir"

    # Initialize Terraform
    log_info "Initializing Terraform..."
    terraform init

    # Plan deployment
    log_info "Planning deployment..."
    terraform plan -var-file="$tfvars_file" -out=tfplan

    if [[ "$FORCE" != "true" ]]; then
        read -p "Do you want to proceed with the deployment? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_warning "Deployment cancelled by user"
            exit 0
        fi
    fi

    # Apply deployment
    log_info "Applying deployment..."
    terraform apply tfplan

    # Platform-specific post-deployment steps
    case $platform in
        hashicorp)
            log_info "Running HashiCorp stack initialization..."
            bash "$PROJECT_ROOT/hashicorp/scripts/setup-cluster.sh" "$environment"
            ;;
    esac

    log_success "Deployment completed successfully"
}

# Destroy infrastructure
destroy() {
    local platform=$1
    local environment=$2
    local tfvars_file="$SCRIPT_DIR/$environment/$platform.tfvars"
    local terraform_dir="$PROJECT_ROOT/$platform"

    log_warning "Destroying $platform/$environment environment..."

    if [[ "$FORCE" != "true" ]]; then
        read -p "Are you sure you want to destroy the $environment environment? This action cannot be undone. (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_warning "Destruction cancelled by user"
            exit 0
        fi
    fi

    cd "$terraform_dir"

    # Initialize Terraform
    terraform init

    # Destroy infrastructure
    terraform destroy -var-file="$tfvars_file" -auto-approve

    log_success "Infrastructure destroyed successfully"
}

# Main function
main() {
    local platform=""
    local environment=""
    local action="deploy"
    local validate_only=false

    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -p|--platform)
                platform="$2"
                shift 2
                ;;
            -e|--environment)
                environment="$2"
                shift 2
                ;;
            -d|--destroy)
                action="destroy"
                shift
                ;;
            -f|--force)
                FORCE="true"
                shift
                ;;
            -v|--validate)
                validate_only=true
                shift
                ;;
            -h|--help)
                usage
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                ;;
        esac
    done

    # Validate required parameters
    if [[ -z "$platform" || -z "$environment" ]]; then
        log_error "Platform and environment are required"
        usage
    fi

    # Validate platform
    if [[ ! "$platform" =~ ^(aws|azure|gcp|hashicorp)$ ]]; then
        log_error "Invalid platform: $platform"
        usage
    fi

    # Validate environment
    if [[ ! "$environment" =~ ^(dev|staging|prod)$ ]]; then
        log_error "Invalid environment: $environment"
        usage
    fi

    # Set default values
    FORCE=${FORCE:-false}

    log_info "Starting deployment process..."
    log_info "Platform: $platform"
    log_info "Environment: $environment"
    log_info "Action: $action"

    # Validate prerequisites
    validate_prerequisites "$platform"

    # Validate configuration
    validate_config "$platform" "$environment"

    if [[ "$validate_only" == "true" ]]; then
        log_success "Validation completed successfully"
        exit 0
    fi

    # Execute action
    case $action in
        deploy)
            deploy "$platform" "$environment"
            ;;
        destroy)
            destroy "$platform" "$environment"
            ;;
    esac

    log_success "Operation completed successfully"
}

# Execute main function
main "$@"