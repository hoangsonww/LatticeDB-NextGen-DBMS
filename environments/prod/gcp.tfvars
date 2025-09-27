# Production environment variables for GCP
project_id = "latticedb-prod-project"
region = "us-east1"
zone = "us-east1-a"
environment = "prod"

# Cloud Run Configuration
service_name = "latticedb-prod"
container_image = "gcr.io/latticedb-prod-project/latticedb:latest"
service_min_instances = 3
service_max_instances = 20
cpu_limit = "4000m"
memory_limit = "4Gi"
cpu_request = "2000m"
memory_request = "2Gi"
max_concurrent_requests = 500

# Network Configuration
vpc_name = "latticedb-prod-vpc"
subnet_name = "latticedb-prod-subnet"
subnet_cidr = "10.1.0.0/24"

# Artifact Registry
artifact_registry_name = "latticedb-prod"
artifact_registry_format = "DOCKER"

# Database Configuration
enable_cloud_sql = true
db_instance_name = "latticedb-prod-db"
db_tier = "db-custom-8-16384"
db_disk_size = 200
db_version = "POSTGRES_13"
db_backup_enabled = true
db_backup_start_time = "02:00"
db_point_in_time_recovery = true
db_high_availability = true

# Secret Manager
enable_secret_manager = true

# Monitoring Configuration
enable_monitoring = true
enable_logging = true
log_retention_days = 90
enable_alerting = true
enable_uptime_checks = true

# IAM Configuration
enable_workload_identity = true

# Security Configuration
enable_binary_authorization = true
enable_private_cluster = true
enable_private_google_access = true

# Load Balancer Configuration
enable_ssl = true
enable_cdn = true
enable_armor = true

# Backup Configuration
enable_automated_backup = true
backup_retention_days = 30

# Labels
labels = {
  environment = "prod"
  project     = "latticedb"
  owner       = "ops-team"
  cost-center = "engineering"
  compliance  = "required"
  backup      = "critical"
}