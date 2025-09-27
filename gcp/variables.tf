variable "project_id" {
  description = "GCP Project ID"
  type        = string
}

variable "gcp_region" {
  description = "GCP region for all resources"
  type        = string
  default     = "us-central1"
}

variable "project_name" {
  description = "Name of the project"
  type        = string
  default     = "latticedb"
}

variable "environment" {
  description = "Environment (e.g., production, staging, development)"
  type        = string
  default     = "production"
}

variable "subnet_cidr" {
  description = "CIDR block for the subnet"
  type        = string
  default     = "10.0.0.0/24"
}

variable "vpc_connector_cidr" {
  description = "CIDR block for VPC connector"
  type        = string
  default     = "10.0.1.0/28"
}

variable "cloud_sql_cidr" {
  description = "CIDR block for Cloud SQL private IP range"
  type        = string
  default     = "10.0.2.0/24"
}

variable "cpu_limit" {
  description = "CPU limit for Cloud Run container"
  type        = string
  default     = "2000m"
}

variable "memory_limit" {
  description = "Memory limit for Cloud Run container"
  type        = string
  default     = "2Gi"
}

variable "min_instances" {
  description = "Minimum number of Cloud Run instances"
  type        = number
  default     = 0
}

variable "max_instances" {
  description = "Maximum number of Cloud Run instances"
  type        = number
  default     = 10
}

variable "image_tag" {
  description = "Docker image tag to deploy"
  type        = string
  default     = "latest"
}

variable "log_level" {
  description = "Log level for the application"
  type        = string
  default     = "info"
  validation {
    condition     = contains(["debug", "info", "warn", "error"], var.log_level)
    error_message = "Log level must be one of: debug, info, warn, error."
  }
}

variable "allow_unauthenticated" {
  description = "Allow unauthenticated access to Cloud Run service"
  type        = bool
  default     = true
}

variable "custom_domain" {
  description = "Custom domain name for the application (optional)"
  type        = string
  default     = ""
}

variable "enable_cdn" {
  description = "Enable Cloud CDN for the load balancer"
  type        = bool
  default     = true
}

variable "enable_cloud_sql" {
  description = "Enable Cloud SQL database"
  type        = bool
  default     = false
}

variable "cloud_sql_version" {
  description = "Cloud SQL database version"
  type        = string
  default     = "POSTGRES_15"
  validation {
    condition     = can(regex("^POSTGRES_[0-9]+$", var.cloud_sql_version))
    error_message = "Cloud SQL version must be a valid PostgreSQL version (e.g., POSTGRES_15)."
  }
}

variable "cloud_sql_tier" {
  description = "Cloud SQL machine type"
  type        = string
  default     = "db-f1-micro"
}

variable "cloud_sql_disk_size" {
  description = "Cloud SQL disk size in GB"
  type        = number
  default     = 20
}

variable "db_password" {
  description = "Database password (leave empty to generate random password)"
  type        = string
  default     = ""
  sensitive   = true
}

variable "enable_cloud_storage" {
  description = "Enable Cloud Storage bucket for persistent data"
  type        = bool
  default     = false
}

variable "enable_secret_manager" {
  description = "Enable Secret Manager for storing secrets"
  type        = bool
  default     = true
}

variable "enable_monitoring" {
  description = "Enable monitoring and alerting"
  type        = bool
  default     = true
}

variable "notification_channels" {
  description = "List of notification channel IDs for alerts"
  type        = list(string)
  default     = []
}

variable "backup_retention_days" {
  description = "Number of days to retain backups"
  type        = number
  default     = 7
}

variable "kms_key_name" {
  description = "KMS key name for encryption (optional)"
  type        = string
  default     = ""
}

variable "labels" {
  description = "Additional labels to apply to resources"
  type        = map(string)
  default     = {}
}

variable "enable_vpc_connector" {
  description = "Enable VPC connector for Cloud Run"
  type        = bool
  default     = true
}

variable "vpc_connector_machine_type" {
  description = "Machine type for VPC connector"
  type        = string
  default     = "e2-micro"
  validation {
    condition     = contains(["e2-micro", "e2-standard-4", "f1-micro"], var.vpc_connector_machine_type)
    error_message = "VPC connector machine type must be one of: e2-micro, e2-standard-4, f1-micro."
  }
}

variable "enable_private_cluster" {
  description = "Enable private cluster configuration"
  type        = bool
  default     = true
}

variable "authorized_networks" {
  description = "List of authorized networks for Cloud SQL"
  type        = list(object({
    name  = string
    value = string
  }))
  default = []
}

variable "enable_binary_logging" {
  description = "Enable binary logging for Cloud SQL"
  type        = bool
  default     = true
}

variable "maintenance_window_day" {
  description = "Day of week for maintenance window (1-7, 1=Monday)"
  type        = number
  default     = 7
  validation {
    condition     = var.maintenance_window_day >= 1 && var.maintenance_window_day <= 7
    error_message = "Maintenance window day must be between 1 and 7."
  }
}

variable "maintenance_window_hour" {
  description = "Hour of day for maintenance window (0-23)"
  type        = number
  default     = 3
  validation {
    condition     = var.maintenance_window_hour >= 0 && var.maintenance_window_hour <= 23
    error_message = "Maintenance window hour must be between 0 and 23."
  }
}

variable "deletion_protection" {
  description = "Enable deletion protection for Cloud SQL"
  type        = bool
  default     = true
}

variable "enable_point_in_time_recovery" {
  description = "Enable point-in-time recovery for Cloud SQL"
  type        = bool
  default     = true
}

variable "backup_start_time" {
  description = "Start time for automated backups (HH:MM format)"
  type        = string
  default     = "03:00"
  validation {
    condition     = can(regex("^([01]?[0-9]|2[0-3]):[0-5][0-9]$", var.backup_start_time))
    error_message = "Backup start time must be in HH:MM format."
  }
}

variable "database_flags" {
  description = "Database flags for Cloud SQL"
  type        = map(string)
  default     = {
    log_checkpoints     = "on"
    log_connections     = "on"
    log_disconnections  = "on"
    log_lock_waits      = "on"
    log_min_duration_statement = "1000"
  }
}