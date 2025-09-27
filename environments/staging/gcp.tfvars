# Staging environment variables for GCP
project_id = "latticedb-staging-project"
region = "us-west1"
zone = "us-west1-a"
environment = "staging"

# Cloud Run Configuration
service_name = "latticedb-staging"
container_image = "gcr.io/latticedb-staging-project/latticedb:staging"
service_min_instances = 1
service_max_instances = 5
cpu_limit = "2000m"
memory_limit = "1Gi"
cpu_request = "500m"
memory_request = "512Mi"
max_concurrent_requests = 200

# Network Configuration
vpc_name = "latticedb-staging-vpc"
subnet_name = "latticedb-staging-subnet"
subnet_cidr = "10.2.0.0/24"

# Artifact Registry
artifact_registry_name = "latticedb-staging"
artifact_registry_format = "DOCKER"

# Database Configuration
enable_cloud_sql = true
db_instance_name = "latticedb-staging-db"
db_tier = "db-custom-2-4096"
db_disk_size = 50
db_version = "POSTGRES_13"
db_backup_enabled = true
db_backup_start_time = "03:00"

# Secret Manager
enable_secret_manager = true

# Monitoring Configuration
enable_monitoring = true
enable_logging = true
log_retention_days = 60
enable_alerting = true

# IAM Configuration
enable_workload_identity = true

# Security Configuration
enable_binary_authorization = false
enable_private_cluster = false

# Labels
labels = {
  environment = "staging"
  project     = "latticedb"
  owner       = "qa-team"
  cost-center = "engineering"
}