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
AWS_REGION="${AWS_REGION:-us-west-2}"
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

    if ! command -v aws &> /dev/null; then
        missing_deps+=("aws-cli")
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
        echo "  AWS CLI: https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html"
        echo "  Terraform: https://learn.hashicorp.com/tutorials/terraform/install-cli"
        echo "  Docker: https://docs.docker.com/get-docker/"
        exit 1
    fi

    log_success "All dependencies are installed"
}

check_aws_credentials() {
    log "Checking AWS credentials..."

    if ! aws sts get-caller-identity &> /dev/null; then
        log_error "AWS credentials not configured or invalid"
        echo ""
        echo "Please configure AWS credentials using one of these methods:"
        echo "  aws configure"
        echo "  aws configure sso"
        echo "  export AWS_PROFILE=your-profile"
        exit 1
    fi

    local account_id=$(aws sts get-caller-identity --query Account --output text)
    local current_user=$(aws sts get-caller-identity --query Arn --output text)
    log_success "AWS credentials valid - Account: $account_id, User: $current_user"
}

build_and_push_image() {
    log "Building and pushing Docker image..."

    # Get ECR repository URL
    local ecr_repo=$(terraform output -raw ecr_repository_url 2>/dev/null || echo "")

    if [ -z "$ecr_repo" ]; then
        log_error "ECR repository not found. Please run terraform apply first."
        exit 1
    fi

    # Login to ECR
    log "Logging in to Amazon ECR..."
    aws ecr get-login-password --region "$AWS_REGION" | docker login --username AWS --password-stdin "$ecr_repo"

    # Build image
    log "Building Docker image..."
    cd "$PROJECT_ROOT"
    docker build -t "${ecr_repo}:${IMAGE_TAG}" .

    # Push image
    log "Pushing Docker image to ECR..."
    docker push "${ecr_repo}:${IMAGE_TAG}"

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
    terraform plan -var="aws_region=$AWS_REGION" -var="environment=$ENVIRONMENT" -var="image_tag=$IMAGE_TAG"

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
    terraform apply -auto-approve -var="aws_region=$AWS_REGION" -var="environment=$ENVIRONMENT" -var="image_tag=$IMAGE_TAG"

    log_success "Infrastructure deployed successfully"
}

update_service() {
    log "Updating ECS service..."

    cd "$SCRIPT_DIR"

    local cluster_name=$(terraform output -raw ecs_cluster_name)
    local service_name=$(terraform output -raw ecs_service_name)

    aws ecs update-service \
        --cluster "$cluster_name" \
        --service "$service_name" \
        --force-new-deployment \
        --region "$AWS_REGION" \
        --no-cli-pager

    log_success "ECS service updated successfully"
}

wait_for_deployment() {
    log "Waiting for deployment to complete..."

    cd "$SCRIPT_DIR"

    local cluster_name=$(terraform output -raw ecs_cluster_name)
    local service_name=$(terraform output -raw ecs_service_name)

    aws ecs wait services-stable \
        --cluster "$cluster_name" \
        --services "$service_name" \
        --region "$AWS_REGION"

    log_success "Deployment completed successfully"
}

show_connection_info() {
    log "Deployment Summary:"
    echo ""

    cd "$SCRIPT_DIR"

    local lb_dns=$(terraform output -raw load_balancer_dns_name)
    local app_url=$(terraform output -raw application_url)

    echo "🎉 LatticeDB has been deployed successfully!"
    echo ""
    echo "📡 Connection Information:"
    echo "   Application URL: $app_url"
    echo "   Health Check:    http://$lb_dns/health"
    echo ""
    echo "📊 Monitoring:"
    echo "   CloudWatch Logs: $(terraform output -raw cloudwatch_log_group_name)"
    echo ""
    echo "🔧 Management Commands:"
    echo "   View logs: aws logs tail $(terraform output -raw cloudwatch_log_group_name) --follow --region $AWS_REGION"
    echo "   Scale service: aws ecs update-service --cluster $(terraform output -raw ecs_cluster_name) --service $(terraform output -raw ecs_service_name) --desired-count <count> --region $AWS_REGION"
    echo ""
}

cleanup() {
    log "Cleaning up resources..."

    cd "$SCRIPT_DIR"

    echo ""
    log_warning "This will destroy all AWS resources created by Terraform."
    read -p "Are you sure you want to continue? (y/N): " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_warning "Cleanup cancelled by user"
        exit 0
    fi

    terraform destroy -auto-approve -var="aws_region=$AWS_REGION" -var="environment=$ENVIRONMENT"

    log_success "Resources cleaned up successfully"
}

show_usage() {
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  deploy     - Deploy LatticeDB to AWS (default)"
    echo "  update     - Update the application without changing infrastructure"
    echo "  cleanup    - Destroy all AWS resources"
    echo "  help       - Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  AWS_REGION     - AWS region (default: us-west-2)"
    echo "  ENVIRONMENT    - Deployment environment (default: production)"
    echo "  IMAGE_TAG      - Docker image tag (default: latest)"
    echo ""
    echo "Examples:"
    echo "  $0 deploy"
    echo "  AWS_REGION=us-east-1 ENVIRONMENT=staging $0 deploy"
    echo "  IMAGE_TAG=v1.0.0 $0 update"
}

main() {
    local command="${1:-deploy}"

    case "$command" in
        deploy)
            check_dependencies
            check_aws_credentials
            deploy_infrastructure
            build_and_push_image
            update_service
            wait_for_deployment
            show_connection_info
            ;;
        update)
            check_dependencies
            check_aws_credentials
            build_and_push_image
            update_service
            wait_for_deployment
            show_connection_info
            ;;
        cleanup)
            check_dependencies
            check_aws_credentials
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