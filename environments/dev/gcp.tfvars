# Development environment variables for GCP
project_id = "latticedb-dev-project"
region = "us-central1"
zone = "us-central1-a"
environment = "dev"

# Cloud Run Configuration
service_name = "latticedb-dev"
container_image = "gcr.io/latticedb-dev-project/latticedb:dev"
service_min_instances = 0
service_max_instances = 2
cpu_limit = "1000m"
memory_limit = "512Mi"
cpu_request = "100m"
memory_request = "256Mi"
max_concurrent_requests = 100

# Network Configuration
vpc_name = "latticedb-dev-vpc"
subnet_name = "latticedb-dev-subnet"
subnet_cidr = "10.0.0.0/24"

# Artifact Registry
artifact_registry_name = "latticedb-dev"
artifact_registry_format = "DOCKER"

# Database Configuration
enable_cloud_sql = false
db_instance_name = "latticedb-dev-db"
db_tier = "db-f1-micro"
db_disk_size = 10
db_version = "POSTGRES_13"

# Secret Manager
enable_secret_manager = true

# Monitoring Configuration
enable_monitoring = true
enable_logging = true
log_retention_days = 30

# IAM Configuration
enable_workload_identity = false

# Labels
labels = {
  environment = "dev"
  project     = "latticedb"
  owner       = "dev-team"
  cost-center = "engineering"
}