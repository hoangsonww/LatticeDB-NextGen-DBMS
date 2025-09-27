variable "cluster_name" {
  description = "Name of the ECS cluster"
  type        = string
}

variable "service_name" {
  description = "Name of the ECS service"
  type        = string
}

variable "container_name" {
  description = "Name of the container"
  type        = string
  default     = "latticedb"
}

variable "container_image" {
  description = "Docker image for the container"
  type        = string
}

variable "container_port" {
  description = "Port exposed by the container"
  type        = number
  default     = 8080
}

variable "task_cpu" {
  description = "CPU units for the task (1024 = 1 vCPU)"
  type        = number
  default     = 512

  validation {
    condition     = contains([256, 512, 1024, 2048, 4096], var.task_cpu)
    error_message = "Task CPU must be one of: 256, 512, 1024, 2048, 4096."
  }
}

variable "task_memory" {
  description = "Memory (in MiB) for the task"
  type        = number
  default     = 1024

  validation {
    condition = var.task_memory >= 512 && var.task_memory <= 30720
    error_message = "Task memory must be between 512 and 30720 MiB."
  }
}

variable "desired_count" {
  description = "Desired number of tasks running in the service"
  type        = number
  default     = 1

  validation {
    condition     = var.desired_count >= 0
    error_message = "Desired count must be a non-negative integer."
  }
}

variable "subnet_ids" {
  description = "List of subnet IDs where the service will be deployed"
  type        = list(string)
}

variable "security_group_ids" {
  description = "List of security group IDs for the service"
  type        = list(string)
}

variable "assign_public_ip" {
  description = "Whether to assign a public IP to tasks"
  type        = bool
  default     = false
}

variable "target_group_arn" {
  description = "ARN of the target group for load balancer"
  type        = string
  default     = null
}

variable "capacity_providers" {
  description = "List of capacity providers for the cluster"
  type        = list(string)
  default     = ["FARGATE"]
}

variable "default_capacity_provider_strategy" {
  description = "Default capacity provider strategy for the cluster"
  type = list(object({
    capacity_provider = string
    weight           = number
    base             = number
  }))
  default = []
}

variable "enable_container_insights" {
  description = "Enable CloudWatch Container Insights for the cluster"
  type        = bool
  default     = true
}

variable "log_retention_days" {
  description = "Number of days to retain logs in CloudWatch"
  type        = number
  default     = 7

  validation {
    condition = contains([1, 3, 5, 7, 14, 30, 60, 90, 120, 150, 180, 365, 400, 545, 731, 1827, 3653], var.log_retention_days)
    error_message = "Log retention must be one of the allowed CloudWatch values."
  }
}

variable "environment_variables" {
  description = "Environment variables for the container"
  type        = map(string)
  default     = {}
}

variable "secrets" {
  description = "Secrets for the container from AWS Systems Manager or Secrets Manager"
  type = list(object({
    name      = string
    valueFrom = string
  }))
  default = []
}

variable "health_check_command" {
  description = "Command to run for container health check"
  type        = string
  default     = "curl -f http://localhost:8080/health || exit 1"
}

variable "health_check_interval" {
  description = "Health check interval in seconds"
  type        = number
  default     = 30
}

variable "health_check_timeout" {
  description = "Health check timeout in seconds"
  type        = number
  default     = 5
}

variable "health_check_retries" {
  description = "Number of health check retries"
  type        = number
  default     = 3
}

variable "health_check_start_period" {
  description = "Grace period for health checks in seconds"
  type        = number
  default     = 60
}

variable "deployment_maximum_percent" {
  description = "Maximum percentage of tasks that can run during deployment"
  type        = number
  default     = 200

  validation {
    condition     = var.deployment_maximum_percent >= 100 && var.deployment_maximum_percent <= 200
    error_message = "Deployment maximum percent must be between 100 and 200."
  }
}

variable "deployment_minimum_healthy_percent" {
  description = "Minimum percentage of healthy tasks during deployment"
  type        = number
  default     = 50

  validation {
    condition     = var.deployment_minimum_healthy_percent >= 0 && var.deployment_minimum_healthy_percent <= 100
    error_message = "Deployment minimum healthy percent must be between 0 and 100."
  }
}

variable "enable_deployment_circuit_breaker" {
  description = "Enable deployment circuit breaker"
  type        = bool
  default     = true
}

variable "enable_deployment_rollback" {
  description = "Enable automatic rollback on deployment failure"
  type        = bool
  default     = true
}

variable "enable_auto_scaling" {
  description = "Enable auto scaling for the service"
  type        = bool
  default     = true
}

variable "auto_scaling_min_capacity" {
  description = "Minimum number of tasks for auto scaling"
  type        = number
  default     = 1
}

variable "auto_scaling_max_capacity" {
  description = "Maximum number of tasks for auto scaling"
  type        = number
  default     = 10
}

variable "auto_scaling_cpu_target" {
  description = "Target CPU utilization for auto scaling"
  type        = number
  default     = 70.0

  validation {
    condition     = var.auto_scaling_cpu_target >= 1.0 && var.auto_scaling_cpu_target <= 100.0
    error_message = "CPU target must be between 1.0 and 100.0."
  }
}

variable "auto_scaling_memory_target" {
  description = "Target memory utilization for auto scaling"
  type        = number
  default     = 80.0

  validation {
    condition     = var.auto_scaling_memory_target >= 1.0 && var.auto_scaling_memory_target <= 100.0
    error_message = "Memory target must be between 1.0 and 100.0."
  }
}

variable "auto_scaling_scale_in_cooldown" {
  description = "Cooldown period for scaling in (seconds)"
  type        = number
  default     = 300
}

variable "auto_scaling_scale_out_cooldown" {
  description = "Cooldown period for scaling out (seconds)"
  type        = number
  default     = 300
}

variable "volume_mounts" {
  description = "Volume mounts for the container"
  type = list(object({
    source_volume   = string
    container_path  = string
    read_only       = bool
  }))
  default = []
}

variable "efs_volumes" {
  description = "EFS volumes for the task definition"
  type = list(object({
    name                    = string
    file_system_id          = string
    root_directory          = string
    transit_encryption      = string
    transit_encryption_port = number
    access_point_id         = string
    iam                     = string
  }))
  default = []
}

variable "enable_secrets_manager_access" {
  description = "Enable access to AWS Secrets Manager"
  type        = bool
  default     = false
}

variable "enable_ssm_access" {
  description = "Enable access to AWS Systems Manager Parameter Store"
  type        = bool
  default     = false
}

variable "enable_ecr_access" {
  description = "Enable access to Amazon ECR"
  type        = bool
  default     = true
}

variable "enable_efs_access" {
  description = "Enable access to Amazon EFS"
  type        = bool
  default     = false
}

variable "task_iam_policy_statements" {
  description = "Additional IAM policy statements for the task role"
  type = list(object({
    Effect   = string
    Action   = list(string)
    Resource = list(string)
  }))
  default = []
}

variable "tags" {
  description = "Tags to apply to all resources"
  type        = map(string)
  default     = {}
}