#!/bin/bash

set -e

# Docker Build and Push Script for AWS ECR

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
AWS_REGION="${AWS_REGION:-us-west-2}"
IMAGE_TAG="${IMAGE_TAG:-latest}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

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

# Get ECR repository URL from Terraform output
get_ecr_repository() {
    local terraform_dir="$(dirname "$SCRIPT_DIR")"

    if [ -f "$terraform_dir/terraform.tfstate" ]; then
        cd "$terraform_dir"
        terraform output -raw ecr_repository_url 2>/dev/null || echo ""
    else
        echo ""
    fi
}

# Login to ECR
ecr_login() {
    log "Logging in to Amazon ECR..."

    local ecr_repo=$(get_ecr_repository)
    if [ -z "$ecr_repo" ]; then
        log_error "ECR repository not found. Please run 'terraform apply' first."
        exit 1
    fi

    aws ecr get-login-password --region "$AWS_REGION" | \
        docker login --username AWS --password-stdin "$ecr_repo"

    log_success "Successfully logged in to ECR"
}

# Build Docker image
build_image() {
    log "Building Docker image..."

    cd "$PROJECT_ROOT"

    local ecr_repo=$(get_ecr_repository)
    local build_args=""

    # Add build arguments
    build_args+=" --build-arg BUILD_DATE=$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
    build_args+=" --build-arg VCS_REF=$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"
    build_args+=" --build-arg VERSION=${IMAGE_TAG}"

    # Add labels
    local labels=""
    labels+=" --label org.opencontainers.image.created=$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
    labels+=" --label org.opencontainers.image.version=${IMAGE_TAG}"
    labels+=" --label org.opencontainers.image.revision=$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')"
    labels+=" --label org.opencontainers.image.source=https://github.com/your-org/LatticeDB-DBMS"
    labels+=" --label org.opencontainers.image.title=LatticeDB"
    labels+=" --label org.opencontainers.image.description='Modern, Next-Gen RDBMS'"

    # Build multi-arch image
    docker buildx create --use --name latticedb-builder 2>/dev/null || true

    docker buildx build \
        --platform linux/amd64,linux/arm64 \
        --tag "${ecr_repo}:${IMAGE_TAG}" \
        --tag "${ecr_repo}:latest" \
        $build_args \
        $labels \
        --push \
        .

    log_success "Docker image built and pushed: ${ecr_repo}:${IMAGE_TAG}"
}

# Scan image for vulnerabilities
scan_image() {
    log "Scanning image for vulnerabilities..."

    local ecr_repo=$(get_ecr_repository)
    local repo_name=$(basename "$ecr_repo")

    # Wait a moment for the image to be available
    sleep 10

    # Start scan
    aws ecr start-image-scan \
        --region "$AWS_REGION" \
        --repository-name "$repo_name" \
        --image-id imageTag="$IMAGE_TAG" \
        --output text &>/dev/null || log_warning "Scan may already be in progress"

    # Wait for scan to complete
    local max_attempts=30
    local attempt=0

    while [ $attempt -lt $max_attempts ]; do
        local scan_status=$(aws ecr describe-image-scan-findings \
            --region "$AWS_REGION" \
            --repository-name "$repo_name" \
            --image-id imageTag="$IMAGE_TAG" \
            --query 'imageScanStatus.status' \
            --output text 2>/dev/null || echo "IN_PROGRESS")

        if [ "$scan_status" = "COMPLETE" ]; then
            break
        fi

        log "Scanning... (attempt $((attempt + 1))/$max_attempts)"
        sleep 10
        ((attempt++))
    done

    if [ $attempt -eq $max_attempts ]; then
        log_warning "Scan timeout - you can check results later with:"
        echo "aws ecr describe-image-scan-findings --repository-name $repo_name --image-id imageTag=$IMAGE_TAG"
        return
    fi

    # Get scan results
    local findings=$(aws ecr describe-image-scan-findings \
        --region "$AWS_REGION" \
        --repository-name "$repo_name" \
        --image-id imageTag="$IMAGE_TAG" \
        --output json)

    local critical_count=$(echo "$findings" | jq '.imageScanFindings.findingCounts.CRITICAL // 0')
    local high_count=$(echo "$findings" | jq '.imageScanFindings.findingCounts.HIGH // 0')
    local medium_count=$(echo "$findings" | jq '.imageScanFindings.findingCounts.MEDIUM // 0')
    local low_count=$(echo "$findings" | jq '.imageScanFindings.findingCounts.LOW // 0')

    echo ""
    echo "🔍 Vulnerability Scan Results:"
    echo "   Critical: $critical_count"
    echo "   High:     $high_count"
    echo "   Medium:   $medium_count"
    echo "   Low:      $low_count"

    if [ "$critical_count" -gt 0 ]; then
        log_error "$critical_count critical vulnerabilities found!"
        echo "$findings" | jq -r '.imageScanFindings.findings[] | select(.severity == "CRITICAL") | "- \(.name): \(.description)"'
        exit 1
    elif [ "$high_count" -gt 0 ]; then
        log_warning "$high_count high-severity vulnerabilities found"
    else
        log_success "No critical vulnerabilities found"
    fi
}

# Clean up local images
cleanup() {
    log "Cleaning up local images..."

    # Remove builder
    docker buildx rm latticedb-builder 2>/dev/null || true

    # Prune build cache
    docker system prune -f --filter "label=stage=intermediate" 2>/dev/null || true

    log_success "Cleanup completed"
}

# Show image information
show_image_info() {
    local ecr_repo=$(get_ecr_repository)
    local repo_name=$(basename "$ecr_repo")

    log "Image Information:"
    echo ""

    # Get image details
    aws ecr describe-images \
        --region "$AWS_REGION" \
        --repository-name "$repo_name" \
        --image-ids imageTag="$IMAGE_TAG" \
        --output table \
        --query 'imageDetails[0].{Tag:imageTags[0],Size:imageSizeInBytes,Pushed:imagePushedAt}' 2>/dev/null || \
        log_warning "Could not retrieve image details"

    echo ""
    echo "🐳 Docker Commands:"
    echo "   Pull: docker pull ${ecr_repo}:${IMAGE_TAG}"
    echo "   Run:  docker run -p 8080:8080 ${ecr_repo}:${IMAGE_TAG}"
    echo ""
    echo "🚀 Deployment:"
    echo "   Update ECS: aws ecs update-service --force-new-deployment --service latticedb-production --cluster latticedb-production"
    echo ""
}

# Usage information
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --tag TAG         Docker image tag (default: latest)"
    echo "  --region REGION   AWS region (default: us-west-2)"
    echo "  --no-scan         Skip vulnerability scanning"
    echo "  --no-cleanup      Skip cleanup"
    echo "  --info            Show image information only"
    echo "  --help            Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  AWS_REGION        AWS region"
    echo "  IMAGE_TAG         Docker image tag"
    echo ""
    echo "Examples:"
    echo "  $0                                # Build with default settings"
    echo "  $0 --tag v1.0.0                 # Build with specific tag"
    echo "  $0 --region us-east-1            # Build in different region"
    echo "  IMAGE_TAG=v2.0.0 $0              # Build using environment variable"
}

# Main function
main() {
    local skip_scan=false
    local skip_cleanup=false
    local info_only=false

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --tag)
                IMAGE_TAG="$2"
                shift 2
                ;;
            --region)
                AWS_REGION="$2"
                shift 2
                ;;
            --no-scan)
                skip_scan=true
                shift
                ;;
            --no-cleanup)
                skip_cleanup=true
                shift
                ;;
            --info)
                info_only=true
                shift
                ;;
            --help)
                usage
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done

    echo "🚀 LatticeDB AWS ECR Build & Push"
    echo "=================================="
    echo "Region: $AWS_REGION"
    echo "Tag: $IMAGE_TAG"
    echo ""

    if [ "$info_only" = true ]; then
        show_image_info
        exit 0
    fi

    # Check dependencies
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed"
        exit 1
    fi

    if ! command -v aws &> /dev/null; then
        log_error "AWS CLI is not installed"
        exit 1
    fi

    if ! command -v jq &> /dev/null; then
        log_warning "jq is not installed - vulnerability scanning will be limited"
    fi

    # Execute build pipeline
    ecr_login
    build_image

    if [ "$skip_scan" = false ]; then
        scan_image
    fi

    show_image_info

    if [ "$skip_cleanup" = false ]; then
        cleanup
    fi

    log_success "Build and push completed successfully!"
}

# Handle script interruption
trap 'log_error "Script interrupted"; exit 1' INT TERM

# Run main function
main "$@"