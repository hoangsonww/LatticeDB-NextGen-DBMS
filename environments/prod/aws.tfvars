# Production environment variables for AWS
region = "us-west-2"
environment = "prod"

# ECS Configuration
cluster_name = "latticedb-prod"
service_name = "latticedb-prod"
container_image = "latticedb:latest"
desired_count = 3
task_cpu = 2048
task_memory = 4096

# Network Configuration
vpc_cidr = "10.1.0.0/16"
enable_nat_gateway = true
assign_public_ip = false

# Storage Configuration
bucket_name = "latticedb-prod-storage"
enable_versioning = true
enable_lifecycle_policy = true

# RDS Configuration
db_instance_class = "db.r5.xlarge"
db_allocated_storage = 100
db_max_allocated_storage = 1000
multi_az = true
backup_retention_period = 30
backup_window = "03:00-04:00"
maintenance_window = "sun:04:00-sun:05:00"

# Load Balancer Configuration
enable_deletion_protection = true
enable_cross_zone_load_balancing = true
idle_timeout = 60

# Auto Scaling Configuration
min_capacity = 2
max_capacity = 10
target_cpu_utilization = 70
target_memory_utilization = 80
scale_up_cooldown = 300
scale_down_cooldown = 300

# Monitoring Configuration
enable_cloudwatch_alarms = true
alert_email_addresses = ["ops@latticedb.com", "alerts@latticedb.com"]
log_retention_days = 30
enable_detailed_monitoring = true
enable_container_insights = true

# Security Configuration
enable_encryption_at_rest = true
enable_encryption_in_transit = true
ssl_policy = "ELBSecurityPolicy-TLS-1-2-2017-01"

# Backup Configuration
enable_automated_backups = true
backup_schedule = "cron(0 2 * * ? *)"
backup_retention_days = 30

# Tags
tags = {
  Environment = "prod"
  Project     = "latticedb"
  Owner       = "ops-team"
  CostCenter  = "engineering"
  Compliance  = "required"
}