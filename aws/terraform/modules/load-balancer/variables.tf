variable "alb_name" {
  description = "Name of the Application Load Balancer"
  type        = string
}

variable "target_group_name" {
  description = "Name of the target group"
  type        = string
}

variable "vpc_id" {
  description = "ID of the VPC"
  type        = string
}

variable "subnet_ids" {
  description = "List of subnet IDs for the load balancer"
  type        = list(string)

  validation {
    condition     = length(var.subnet_ids) >= 2
    error_message = "At least 2 subnet IDs must be provided for high availability."
  }
}

variable "security_group_ids" {
  description = "List of security group IDs for the load balancer"
  type        = list(string)
}

variable "internal" {
  description = "Whether the load balancer is internal"
  type        = bool
  default     = false
}

variable "target_port" {
  description = "Port on which targets receive traffic"
  type        = number
  default     = 8080
}

variable "target_protocol" {
  description = "Protocol for routing traffic to targets"
  type        = string
  default     = "HTTP"

  validation {
    condition     = contains(["HTTP", "HTTPS"], var.target_protocol)
    error_message = "Target protocol must be HTTP or HTTPS."
  }
}

variable "target_type" {
  description = "Type of target that you must specify when registering targets"
  type        = string
  default     = "ip"

  validation {
    condition     = contains(["instance", "ip", "lambda", "alb"], var.target_type)
    error_message = "Target type must be instance, ip, lambda, or alb."
  }
}

variable "enable_deletion_protection" {
  description = "Enable deletion protection for the load balancer"
  type        = bool
  default     = false
}

variable "enable_access_logs" {
  description = "Enable access logs for the load balancer"
  type        = bool
  default     = false
}

variable "access_logs_bucket" {
  description = "S3 bucket name for access logs"
  type        = string
  default     = null
}

variable "access_logs_prefix" {
  description = "S3 prefix for access logs"
  type        = string
  default     = "alb-access-logs"
}

variable "health_check_healthy_threshold" {
  description = "Number of consecutive health checks successes required"
  type        = number
  default     = 2
}

variable "health_check_unhealthy_threshold" {
  description = "Number of consecutive health check failures required"
  type        = number
  default     = 2
}

variable "health_check_timeout" {
  description = "Health check timeout in seconds"
  type        = number
  default     = 5
}

variable "health_check_interval" {
  description = "Health check interval in seconds"
  type        = number
  default     = 30
}

variable "health_check_path" {
  description = "Health check path"
  type        = string
  default     = "/health"
}

variable "health_check_matcher" {
  description = "Response codes to use when checking for a healthy responses"
  type        = string
  default     = "200"
}

variable "health_check_port" {
  description = "Port for health checks"
  type        = string
  default     = "traffic-port"
}

variable "health_check_protocol" {
  description = "Protocol for health checks"
  type        = string
  default     = "HTTP"

  validation {
    condition     = contains(["HTTP", "HTTPS"], var.health_check_protocol)
    error_message = "Health check protocol must be HTTP or HTTPS."
  }
}

variable "enable_stickiness" {
  description = "Enable sticky sessions"
  type        = bool
  default     = false
}

variable "stickiness_duration" {
  description = "Cookie duration for sticky sessions in seconds"
  type        = number
  default     = 86400

  validation {
    condition     = var.stickiness_duration >= 1 && var.stickiness_duration <= 604800
    error_message = "Stickiness duration must be between 1 and 604800 seconds."
  }
}

variable "enable_http_listener" {
  description = "Enable HTTP listener (will redirect to HTTPS)"
  type        = bool
  default     = true
}

variable "enable_https_listener" {
  description = "Enable HTTPS listener"
  type        = bool
  default     = false
}

variable "certificate_arn" {
  description = "ARN of the SSL certificate for HTTPS listener"
  type        = string
  default     = null
}

variable "ssl_policy" {
  description = "SSL policy for HTTPS listener"
  type        = string
  default     = "ELBSecurityPolicy-TLS-1-2-2017-01"
}

variable "enable_custom_listener" {
  description = "Enable custom port listener"
  type        = bool
  default     = false
}

variable "custom_listener_port" {
  description = "Port for custom listener"
  type        = number
  default     = 8080
}

variable "custom_listener_protocol" {
  description = "Protocol for custom listener"
  type        = string
  default     = "HTTP"

  validation {
    condition     = contains(["HTTP", "HTTPS", "TCP", "UDP"], var.custom_listener_protocol)
    error_message = "Custom listener protocol must be HTTP, HTTPS, TCP, or UDP."
  }
}

variable "enable_blue_green_deployment" {
  description = "Enable blue/green deployment target group"
  type        = bool
  default     = false
}

variable "enable_api_routing" {
  description = "Enable path-based routing for API endpoints"
  type        = bool
  default     = false
}

variable "api_rule_priority" {
  description = "Priority for API routing rule"
  type        = number
  default     = 100
}

variable "api_path_patterns" {
  description = "Path patterns for API routing"
  type        = list(string)
  default     = ["/api/*"]
}

variable "enable_host_routing" {
  description = "Enable host-based routing"
  type        = bool
  default     = false
}

variable "host_rule_priority" {
  description = "Priority for host routing rule"
  type        = number
  default     = 200
}

variable "host_header_values" {
  description = "Host header values for routing"
  type        = list(string)
  default     = []
}

variable "enable_waf" {
  description = "Enable AWS WAF protection"
  type        = bool
  default     = false
}

variable "waf_acl_arn" {
  description = "ARN of the WAF Web ACL"
  type        = string
  default     = null
}

variable "enable_cloudwatch_alarms" {
  description = "Enable CloudWatch alarms"
  type        = bool
  default     = true
}

variable "alarm_actions" {
  description = "List of alarm actions (SNS topic ARNs)"
  type        = list(string)
  default     = []
}

variable "response_time_threshold" {
  description = "Response time threshold for CloudWatch alarm (seconds)"
  type        = number
  default     = 1.0
}

variable "http_5xx_threshold" {
  description = "HTTP 5XX error threshold for CloudWatch alarm"
  type        = number
  default     = 10
}

variable "enable_route53_health_check" {
  description = "Enable Route53 health check"
  type        = bool
  default     = false
}

variable "health_check_fqdn" {
  description = "FQDN for Route53 health check"
  type        = string
  default     = null
}

variable "route53_failure_threshold" {
  description = "Failure threshold for Route53 health check"
  type        = number
  default     = 3
}

variable "route53_request_interval" {
  description = "Request interval for Route53 health check (seconds)"
  type        = number
  default     = 30

  validation {
    condition     = contains([10, 30], var.route53_request_interval)
    error_message = "Route53 request interval must be 10 or 30 seconds."
  }
}

variable "tags" {
  description = "Tags to apply to all resources"
  type        = map(string)
  default     = {}
}