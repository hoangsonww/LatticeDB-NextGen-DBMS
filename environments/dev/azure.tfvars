# Development environment variables for Azure
location = "East US"
environment = "dev"

# Resource Group
resource_group_name = "latticedb-dev-rg"

# Container Apps Configuration
container_app_name = "latticedb-dev"
container_image = "latticedb:dev"
min_replicas = 1
max_replicas = 2
cpu_requests = "0.25"
memory_requests = "0.5Gi"
cpu_limits = "0.5"
memory_limits = "1Gi"

# Network Configuration
vnet_address_space = ["10.0.0.0/16"]
subnet_address_prefixes = ["10.0.1.0/24"]

# Container Registry
acr_name = "latticedbdevacr"
acr_sku = "Basic"

# Key Vault Configuration
key_vault_name = "latticedb-dev-kv"
key_vault_sku = "standard"

# Database Configuration
enable_postgres = false
postgres_sku = "B_Gen5_1"
postgres_storage_mb = 5120
postgres_version = "13"

# Monitoring Configuration
enable_app_insights = true
app_insights_type = "web"
log_analytics_retention_days = 30

# Tags
tags = {
  Environment = "dev"
  Project     = "latticedb"
  Owner       = "dev-team"
  CostCenter  = "engineering"
}