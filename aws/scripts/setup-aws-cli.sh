#!/bin/bash

set -e

# AWS CLI Setup Script for LatticeDB Deployment

# Colors for output
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

# Function to install AWS CLI
install_aws_cli() {
    log "Installing AWS CLI..."

    if command -v aws &> /dev/null; then
        log_success "AWS CLI already installed: $(aws --version)"
        return 0
    fi

    # Detect OS
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Linux
        log "Installing AWS CLI for Linux..."
        curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
        unzip -q awscliv2.zip
        sudo ./aws/install
        rm -rf aws awscliv2.zip
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        log "Installing AWS CLI for macOS..."
        if command -v brew &> /dev/null; then
            brew install awscli
        else
            curl "https://awscli.amazonaws.com/AWSCLIV2.pkg" -o "AWSCLIV2.pkg"
            sudo installer -pkg AWSCLIV2.pkg -target /
            rm AWSCLIV2.pkg
        fi
    else
        log_error "Unsupported operating system: $OSTYPE"
        exit 1
    fi

    log_success "AWS CLI installed: $(aws --version)"
}

# Function to configure AWS credentials
configure_aws() {
    log "Configuring AWS credentials..."

    if aws sts get-caller-identity &> /dev/null; then
        log_success "AWS credentials already configured"
        local identity=$(aws sts get-caller-identity)
        echo "Current AWS identity:"
        echo "$identity" | jq '.'
        return 0
    fi

    log_warning "AWS credentials not configured"
    echo ""
    echo "Please configure AWS credentials using one of these methods:"
    echo ""
    echo "1. AWS CLI configuration:"
    echo "   aws configure"
    echo ""
    echo "2. AWS SSO:"
    echo "   aws configure sso"
    echo ""
    echo "3. Environment variables:"
    echo "   export AWS_ACCESS_KEY_ID=your-access-key"
    echo "   export AWS_SECRET_ACCESS_KEY=your-secret-key"
    echo "   export AWS_DEFAULT_REGION=us-west-2"
    echo ""
    echo "4. IAM roles (if running on EC2):"
    echo "   Attach an IAM role to your EC2 instance"
    echo ""

    read -p "Would you like to configure AWS CLI now? (y/N): " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        aws configure
    fi
}

# Function to verify permissions
verify_permissions() {
    log "Verifying AWS permissions..."

    local required_permissions=(
        "sts:GetCallerIdentity"
        "ec2:DescribeVpcs"
        "ecs:ListClusters"
        "ecr:GetAuthorizationToken"
        "iam:GetRole"
        "logs:DescribeLogGroups"
    )

    local failed_checks=0

    # Test basic identity
    if aws sts get-caller-identity &> /dev/null; then
        log_success "✓ STS GetCallerIdentity"
    else
        log_error "✗ STS GetCallerIdentity"
        ((failed_checks++))
    fi

    # Test EC2 permissions
    if aws ec2 describe-vpcs --max-items 1 &> /dev/null; then
        log_success "✓ EC2 DescribeVpcs"
    else
        log_error "✗ EC2 DescribeVpcs"
        ((failed_checks++))
    fi

    # Test ECS permissions
    if aws ecs list-clusters --max-items 1 &> /dev/null; then
        log_success "✓ ECS ListClusters"
    else
        log_error "✗ ECS ListClusters"
        ((failed_checks++))
    fi

    # Test ECR permissions
    if aws ecr get-authorization-token --output text &> /dev/null; then
        log_success "✓ ECR GetAuthorizationToken"
    else
        log_error "✗ ECR GetAuthorizationToken"
        ((failed_checks++))
    fi

    if [ $failed_checks -eq 0 ]; then
        log_success "All permission checks passed"
    else
        log_warning "$failed_checks permission checks failed"
        echo ""
        echo "Required IAM policy for LatticeDB deployment:"
        cat << 'EOF'
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "ec2:*",
                "ecs:*",
                "ecr:*",
                "efs:*",
                "elasticloadbalancing:*",
                "application-autoscaling:*",
                "iam:*",
                "logs:*",
                "cloudwatch:*",
                "ssm:*",
                "secretsmanager:*"
            ],
            "Resource": "*"
        }
    ]
}
EOF
    fi
}

# Function to setup S3 backend for Terraform state
setup_s3_backend() {
    local bucket_name="${1:-latticedb-terraform-state-$(openssl rand -hex 8)}"
    local region="${2:-us-west-2}"

    log "Setting up S3 backend for Terraform state..."

    # Check if bucket already exists
    if aws s3api head-bucket --bucket "$bucket_name" 2>/dev/null; then
        log_success "S3 bucket $bucket_name already exists"
    else
        log "Creating S3 bucket: $bucket_name"

        if [ "$region" == "us-east-1" ]; then
            aws s3api create-bucket --bucket "$bucket_name"
        else
            aws s3api create-bucket --bucket "$bucket_name" \
                --create-bucket-configuration LocationConstraint="$region"
        fi

        # Enable versioning
        aws s3api put-bucket-versioning --bucket "$bucket_name" \
            --versioning-configuration Status=Enabled

        # Enable server-side encryption
        aws s3api put-bucket-encryption --bucket "$bucket_name" \
            --server-side-encryption-configuration '{
                "Rules": [{
                    "ApplyServerSideEncryptionByDefault": {
                        "SSEAlgorithm": "AES256"
                    }
                }]
            }'

        log_success "S3 bucket created: $bucket_name"
    fi

    # Create DynamoDB table for state locking
    local table_name="${bucket_name}-lock"

    if aws dynamodb describe-table --table-name "$table_name" &> /dev/null; then
        log_success "DynamoDB table $table_name already exists"
    else
        log "Creating DynamoDB table for state locking: $table_name"

        aws dynamodb create-table \
            --table-name "$table_name" \
            --attribute-definitions AttributeName=LockID,AttributeType=S \
            --key-schema AttributeName=LockID,KeyType=HASH \
            --provisioned-throughput ReadCapacityUnits=1,WriteCapacityUnits=1 \
            --tags Key=Project,Value=LatticeDB Key=Purpose,Value=TerraformStateLock

        # Wait for table to be active
        log "Waiting for DynamoDB table to be active..."
        aws dynamodb wait table-exists --table-name "$table_name"

        log_success "DynamoDB table created: $table_name"
    fi

    # Output backend configuration
    echo ""
    log_success "Terraform backend configuration:"
    cat << EOF
terraform {
  backend "s3" {
    bucket         = "$bucket_name"
    key            = "latticedb/terraform.tfstate"
    region         = "$region"
    dynamodb_table = "$table_name"
    encrypt        = true
  }
}
EOF
}

# Main function
main() {
    echo "🚀 LatticeDB AWS Setup Script"
    echo "================================"
    echo ""

    install_aws_cli
    configure_aws
    verify_permissions

    echo ""
    read -p "Would you like to set up S3 backend for Terraform state? (y/N): " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        read -p "Enter S3 bucket name (or press Enter for auto-generated): " bucket_name
        read -p "Enter AWS region (default: us-west-2): " region
        region=${region:-us-west-2}

        setup_s3_backend "$bucket_name" "$region"
    fi

    echo ""
    log_success "AWS setup completed!"
    echo ""
    echo "Next steps:"
    echo "1. Copy and customize terraform.tfvars.example to terraform.tfvars"
    echo "2. Run './deploy.sh' to deploy LatticeDB"
}

# Run main function
main "$@"