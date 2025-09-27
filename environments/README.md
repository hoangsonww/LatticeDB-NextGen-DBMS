# LatticeDB Environment Configurations

This directory contains environment-specific configurations for deploying LatticeDB across multiple cloud platforms and environments.

## Directory Structure

```
environments/
├── dev/                    # Development environment configurations
│   ├── aws.tfvars         # AWS development variables
│   ├── azure.tfvars       # Azure development variables
│   ├── gcp.tfvars         # GCP development variables
│   └── hashicorp.tfvars   # HashiCorp stack development variables
├── staging/               # Staging environment configurations
│   ├── aws.tfvars         # AWS staging variables
│   ├── azure.tfvars       # Azure staging variables
│   ├── gcp.tfvars         # GCP staging variables
│   └── hashicorp.tfvars   # HashiCorp stack staging variables
├── prod/                  # Production environment configurations
│   ├── aws.tfvars         # AWS production variables
│   ├── azure.tfvars       # Azure production variables
│   ├── gcp.tfvars         # GCP production variables
│   └── hashicorp.tfvars   # HashiCorp stack production variables
└── deploy.sh             # Universal deployment script
```

## Supported Platforms

### AWS (Amazon Web Services)
- **Service**: Amazon ECS with Fargate
- **Features**: Auto-scaling, Load Balancer, RDS, S3, CloudWatch
- **Networking**: VPC with public/private subnets, NAT Gateway

### Azure (Microsoft Azure)
- **Service**: Container Apps
- **Features**: Auto-scaling, Container Registry, PostgreSQL, Key Vault
- **Networking**: VNet with subnet delegation, Private Endpoints

### GCP (Google Cloud Platform)
- **Service**: Cloud Run
- **Features**: Auto-scaling, Artifact Registry, Cloud SQL, Secret Manager
- **Networking**: VPC with Cloud NAT, Private Google Access

### HashiCorp Stack
- **Services**: Nomad, Consul, Vault
- **Features**: Service Mesh, Secret Management, Job Orchestration
- **Networking**: Consul Connect with Envoy proxies

## Environment Characteristics

### Development (dev)
- **Purpose**: Local development and testing
- **Resources**: Minimal (1 instance, basic specs)
- **Monitoring**: Basic logging, 7-30 day retention
- **Security**: Simplified (no encryption for HashiCorp)
- **Cost**: Optimized for minimal spend

### Staging (staging)
- **Purpose**: Pre-production testing and QA
- **Resources**: Moderate (2-3 instances, mid-tier specs)
- **Monitoring**: Enhanced logging, alerting enabled
- **Security**: Production-like (TLS, ACLs enabled)
- **Cost**: Balanced between features and cost

### Production (prod)
- **Purpose**: Live production workloads
- **Resources**: High availability (3+ instances, high-performance specs)
- **Monitoring**: Full observability, alerting, uptime checks
- **Security**: Maximum (encryption, ACLs, compliance features)
- **Cost**: Performance and reliability prioritized

## Deployment Guide

### Prerequisites

Install required tools based on your target platform:

**AWS:**
```bash
# AWS CLI
curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
unzip awscliv2.zip
sudo ./aws/install

# Configure AWS credentials
aws configure
```

**Azure:**
```bash
# Azure CLI
curl -sL https://aka.ms/InstallAzureCLIDeb | sudo bash

# Login to Azure
az login
```

**GCP:**
```bash
# Google Cloud CLI
curl https://sdk.cloud.google.com | bash
exec -l $SHELL

# Authenticate
gcloud auth login
gcloud config set project YOUR-PROJECT-ID
```

**HashiCorp:**
```bash
# Install HashiCorp tools (example for Ubuntu)
wget -O- https://apt.releases.hashicorp.com/gpg | gpg --dearmor | sudo tee /usr/share/keyrings/hashicorp-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/hashicorp-archive-keyring.gpg] https://apt.releases.hashicorp.com $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/hashicorp.list
sudo apt update && sudo apt install terraform nomad consul vault
```

**All Platforms:**
```bash
# Terraform (if not installed via HashiCorp repository)
wget https://releases.hashicorp.com/terraform/1.6.0/terraform_1.6.0_linux_amd64.zip
unzip terraform_1.6.0_linux_amd64.zip
sudo mv terraform /usr/local/bin/
```

### Quick Start

1. **Deploy to Development Environment:**
   ```bash
   ./deploy.sh -p aws -e dev
   ```

2. **Deploy to Staging with Validation:**
   ```bash
   ./deploy.sh -p azure -e staging --validate
   ./deploy.sh -p azure -e staging
   ```

3. **Deploy to Production (with confirmation):**
   ```bash
   ./deploy.sh -p gcp -e prod
   ```

4. **Destroy Environment:**
   ```bash
   ./deploy.sh -p hashicorp -e dev --destroy
   ```

### Advanced Usage

**Validate Configuration Only:**
```bash
./deploy.sh -p aws -e prod --validate
```

**Force Deployment (skip confirmations):**
```bash
./deploy.sh -p azure -e dev --force
```

**Platform-Specific Deployment:**
```bash
# Deploy HashiCorp stack
./deploy.sh -p hashicorp -e staging

# Deploy to multiple platforms
for platform in aws azure gcp; do
    ./deploy.sh -p $platform -e dev --force
done
```

## Configuration Customization

### Modifying Environment Variables

1. **Edit the appropriate .tfvars file:**
   ```bash
   vim environments/prod/aws.tfvars
   ```

2. **Key variables to customize:**
   - `region`: Deployment region
   - `desired_count`: Number of instances
   - `cpu`/`memory`: Resource allocation
   - `tags`: Resource tagging
   - `enable_*`: Feature toggles

3. **Validate changes:**
   ```bash
   ./deploy.sh -p aws -e prod --validate
   ```

### Adding New Environments

1. **Create new directory:**
   ```bash
   mkdir environments/test
   ```

2. **Copy and modify configuration:**
   ```bash
   cp environments/dev/*.tfvars environments/test/
   # Edit files as needed
   ```

3. **Update deployment script** (if needed) to support the new environment.

## Security Considerations

### Development Environment
- Public subnets allowed for easier access
- Basic authentication
- Minimal encryption
- Cost-optimized resources

### Staging Environment
- Private subnets preferred
- TLS/SSL enabled
- Database encryption
- Moderate backup retention

### Production Environment
- Private subnets mandatory
- Full encryption (in-transit and at-rest)
- Advanced threat protection
- Extended backup retention
- Compliance features enabled

## Monitoring and Alerting

Each environment includes monitoring configurations:

- **CloudWatch/Azure Monitor/Stackdriver**: Platform-native monitoring
- **Prometheus + Grafana**: Custom metrics (HashiCorp stack)
- **AlertManager**: Alert routing and management
- **Log Aggregation**: Centralized logging with retention policies

### Alert Channels

- **Dev**: Email notifications to dev team
- **Staging**: Email + Slack notifications
- **Prod**: Email + Slack + PagerDuty (critical alerts)

## Cost Optimization

### Development
- Minimum viable resources
- Shorter retention periods
- Basic monitoring
- No high availability

### Staging
- Balanced resource allocation
- Moderate retention periods
- Enhanced monitoring
- Limited high availability

### Production
- Performance-optimized resources
- Extended retention periods
- Comprehensive monitoring
- Full high availability and disaster recovery

## Troubleshooting

### Common Issues

1. **Authentication Errors:**
   ```bash
   # Check AWS credentials
   aws sts get-caller-identity

   # Check Azure login
   az account show

   # Check GCP authentication
   gcloud auth list
   ```

2. **Terraform State Issues:**
   ```bash
   # Initialize backend
   cd <platform-directory>
   terraform init

   # Refresh state
   terraform refresh -var-file="../environments/<env>/<platform>.tfvars"
   ```

3. **Resource Limit Errors:**
   - Check cloud provider quotas
   - Adjust resource requests in tfvars
   - Consider different regions/zones

4. **Network Connectivity:**
   - Verify VPC/VNet configuration
   - Check security group/NSG rules
   - Validate DNS resolution

### Support

For deployment issues:
1. Check the deployment logs
2. Validate configuration with `--validate` flag
3. Review cloud provider documentation
4. Open an issue in the project repository

## Migration Between Environments

### Promoting from Dev to Staging
```bash
# Export data from dev
./environments/scripts/export-data.sh dev

# Deploy to staging
./deploy.sh -p <platform> -e staging

# Import data to staging
./environments/scripts/import-data.sh staging
```

### Blue-Green Deployment for Production
```bash
# Deploy to new infrastructure
./deploy.sh -p <platform> -e prod-blue

# Test and validate
./environments/scripts/validate-deployment.sh prod-blue

# Switch traffic (platform-specific)
./environments/scripts/switch-traffic.sh prod-green prod-blue

# Cleanup old environment
./deploy.sh -p <platform> -e prod-green --destroy
```