variable "sns_topic_name" {
  description = "Name of the SNS topic for alerts"
  type        = string
  default     = "latticedb-alerts"
}

variable "dashboard_name" {
  description = "Name of the CloudWatch dashboard"
  type        = string
  default     = "LatticeDB-Dashboard"
}

variable "alert_email_addresses" {
  description = "List of email addresses to receive alerts"
  type        = list(string)
  default     = []
}

variable "slack_webhook_url" {
  description = "Slack webhook URL for notifications"
  type        = string
  default     = null
}

variable "enable_ecs_monitoring" {
  description = "Enable ECS service monitoring"
  type        = bool
  default     = true
}

variable "ecs_service_name" {
  description = "Name of the ECS service to monitor"
  type        = string
  default     = ""
}

variable "ecs_cluster_name" {
  description = "Name of the ECS cluster"
  type        = string
  default     = ""
}

variable "ecs_cluster_arn" {
  description = "ARN of the ECS cluster"
  type        = string
  default     = ""
}

variable "ecs_cpu_threshold" {
  description = "CPU utilization threshold for ECS alarm (percentage)"
  type        = number
  default     = 80

  validation {
    condition     = var.ecs_cpu_threshold >= 0 && var.ecs_cpu_threshold <= 100
    error_message = "ECS CPU threshold must be between 0 and 100."
  }
}

variable "ecs_memory_threshold" {
  description = "Memory utilization threshold for ECS alarm (percentage)"
  type        = number
  default     = 80

  validation {
    condition     = var.ecs_memory_threshold >= 0 && var.ecs_memory_threshold <= 100
    error_message = "ECS memory threshold must be between 0 and 100."
  }
}

variable "ecs_min_running_tasks" {
  description = "Minimum number of running tasks for ECS service"
  type        = number
  default     = 1
}

variable "enable_alb_monitoring" {
  description = "Enable Application Load Balancer monitoring"
  type        = bool
  default     = false
}

variable "alb_name" {
  description = "Name of the ALB to monitor"
  type        = string
  default     = ""
}

variable "target_group_name" {
  description = "Name of the target group to monitor"
  type        = string
  default     = ""
}

variable "alb_response_time_threshold" {
  description = "Response time threshold for ALB alarm (seconds)"
  type        = number
  default     = 1.0
}

variable "alb_5xx_threshold" {
  description = "5XX error count threshold for ALB alarm"
  type        = number
  default     = 10
}

variable "enable_rds_monitoring" {
  description = "Enable RDS monitoring"
  type        = bool
  default     = false
}

variable "rds_instance_identifier" {
  description = "RDS instance identifier to monitor"
  type        = string
  default     = ""
}

variable "rds_cpu_threshold" {
  description = "CPU utilization threshold for RDS alarm (percentage)"
  type        = number
  default     = 80

  validation {
    condition     = var.rds_cpu_threshold >= 0 && var.rds_cpu_threshold <= 100
    error_message = "RDS CPU threshold must be between 0 and 100."
  }
}

variable "rds_connections_threshold" {
  description = "Database connections threshold for RDS alarm"
  type        = number
  default     = 80
}

variable "rds_free_storage_threshold" {
  description = "Free storage space threshold for RDS alarm (bytes)"
  type        = number
  default     = 2000000000  # 2GB in bytes
}

variable "enable_custom_metrics" {
  description = "Enable custom application metrics"
  type        = bool
  default     = true
}

variable "error_log_pattern" {
  description = "Log pattern to match application errors"
  type        = string
  default     = "[ERROR]"
}

variable "application_error_threshold" {
  description = "Application error count threshold for alarm"
  type        = number
  default     = 10
}

variable "log_retention_days" {
  description = "Number of days to retain CloudWatch logs"
  type        = number
  default     = 14

  validation {
    condition = contains([1, 3, 5, 7, 14, 30, 60, 90, 120, 150, 180, 365, 400, 545, 731, 1827, 3653], var.log_retention_days)
    error_message = "Log retention must be one of the allowed CloudWatch values."
  }
}

variable "enable_xray_tracing" {
  description = "Enable AWS X-Ray tracing"
  type        = bool
  default     = false
}

variable "tags" {
  description = "Tags to apply to all resources"
  type        = map(string)
  default     = {}
}