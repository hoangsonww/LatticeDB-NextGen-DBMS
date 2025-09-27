# Staging environment variables for Azure
location = "West US 2"
environment = "staging"

# Resource Group
resource_group_name = "latticedb-staging-rg"

# Container Apps Configuration
container_app_name = "latticedb-staging"
container_image = "latticedb:staging"
min_replicas = 2
max_replicas = 5
cpu_requests = "0.5"
memory_requests = "1Gi"
cpu_limits = "1"
memory_limits = "2Gi"

# Network Configuration
vnet_address_space = ["10.2.0.0/16"]
subnet_address_prefixes = ["10.2.1.0/24"]

# Container Registry
acr_name = "latticedbstagingacr"
acr_sku = "Standard"

# Key Vault Configuration
key_vault_name = "latticedb-staging-kv"
key_vault_sku = "standard"

# Database Configuration
enable_postgres = true
postgres_sku = "GP_Gen5_2"
postgres_storage_mb = 51200
postgres_version = "13"
postgres_backup_retention_days = 7

# Monitoring Configuration
enable_app_insights = true
app_insights_type = "web"
log_analytics_retention_days = 60
enable_alerts = true

# Security Configuration
enable_private_endpoints = true

# Tags
tags = {
  Environment = "staging"
  Project     = "latticedb"
  Owner       = "qa-team"
  CostCenter  = "engineering"
}