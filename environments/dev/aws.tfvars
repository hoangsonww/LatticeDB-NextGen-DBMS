# Development environment variables for AWS
region = "us-east-1"
environment = "dev"

# ECS Configuration
cluster_name = "latticedb-dev"
service_name = "latticedb-dev"
container_image = "latticedb:dev"
desired_count = 1
task_cpu = 256
task_memory = 512

# Network Configuration
vpc_cidr = "10.0.0.0/16"
enable_nat_gateway = false
assign_public_ip = true

# Storage Configuration
bucket_name = "latticedb-dev-storage"
enable_versioning = false
enable_lifecycle_policy = false

# Monitoring Configuration
enable_cloudwatch_alarms = true
alert_email_addresses = ["dev@example.com"]
log_retention_days = 7

# Tags
tags = {
  Environment = "dev"
  Project     = "latticedb"
  Owner       = "dev-team"
}