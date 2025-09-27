# Production environment variables for Azure
location = "East US 2"
environment = "prod"

# Resource Group
resource_group_name = "latticedb-prod-rg"

# Container Apps Configuration
container_app_name = "latticedb-prod"
container_image = "latticedb:latest"
min_replicas = 3
max_replicas = 10
cpu_requests = "1"
memory_requests = "2Gi"
cpu_limits = "2"
memory_limits = "4Gi"

# Network Configuration
vnet_address_space = ["10.1.0.0/16"]
subnet_address_prefixes = ["10.1.1.0/24"]

# Container Registry
acr_name = "latticedbprodacr"
acr_sku = "Premium"

# Key Vault Configuration
key_vault_name = "latticedb-prod-kv"
key_vault_sku = "premium"

# Database Configuration
enable_postgres = true
postgres_sku = "GP_Gen5_4"
postgres_storage_mb = 102400
postgres_version = "13"
postgres_backup_retention_days = 35
postgres_geo_redundant_backup = true

# Monitoring Configuration
enable_app_insights = true
app_insights_type = "web"
log_analytics_retention_days = 90
enable_alerts = true
enable_autoscale_notifications = true

# Security Configuration
enable_private_endpoints = true
enable_firewall = true
enable_ddos_protection = true

# Backup Configuration
enable_backup = true
backup_policy_type = "V2"
backup_frequency = "Daily"
backup_retention_days = 30

# Tags
tags = {
  Environment = "prod"
  Project     = "latticedb"
  Owner       = "ops-team"
  CostCenter  = "engineering"
  Compliance  = "required"
  Backup      = "critical"
}