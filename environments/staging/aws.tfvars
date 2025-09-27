# Staging environment variables for AWS
region = "us-west-1"
environment = "staging"

# ECS Configuration
cluster_name = "latticedb-staging"
service_name = "latticedb-staging"
container_image = "latticedb:staging"
desired_count = 2
task_cpu = 1024
task_memory = 2048

# Network Configuration
vpc_cidr = "10.2.0.0/16"
enable_nat_gateway = true
assign_public_ip = false

# Storage Configuration
bucket_name = "latticedb-staging-storage"
enable_versioning = true
enable_lifecycle_policy = true

# RDS Configuration
db_instance_class = "db.t3.large"
db_allocated_storage = 50
db_max_allocated_storage = 200
multi_az = false
backup_retention_period = 7
backup_window = "03:00-04:00"
maintenance_window = "sun:04:00-sun:05:00"

# Load Balancer Configuration
enable_deletion_protection = false
enable_cross_zone_load_balancing = true
idle_timeout = 60

# Auto Scaling Configuration
min_capacity = 1
max_capacity = 5
target_cpu_utilization = 70
target_memory_utilization = 80
scale_up_cooldown = 300
scale_down_cooldown = 300

# Monitoring Configuration
enable_cloudwatch_alarms = true
alert_email_addresses = ["staging-alerts@latticedb.com"]
log_retention_days = 14
enable_detailed_monitoring = true
enable_container_insights = true

# Security Configuration
enable_encryption_at_rest = true
enable_encryption_in_transit = true
ssl_policy = "ELBSecurityPolicy-TLS-1-2-2017-01"

# Backup Configuration
enable_automated_backups = true
backup_schedule = "cron(0 3 * * ? *)"
backup_retention_days = 7

# Tags
tags = {
  Environment = "staging"
  Project     = "latticedb"
  Owner       = "qa-team"
  CostCenter  = "engineering"
}