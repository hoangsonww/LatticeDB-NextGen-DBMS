# Monitoring Configuration Variables for Azure

variable "enable_monitoring" {
  description = "Enable Prometheus and Grafana monitoring stack"
  type        = bool
  default     = true
}

variable "prometheus_cpu" {
  description = "CPU allocation for Prometheus container"
  type        = number
  default     = 0.5
}

variable "prometheus_memory" {
  description = "Memory allocation for Prometheus container in GB"
  type        = number
  default     = 1
}

variable "prometheus_retention" {
  description = "Data retention period for Prometheus"
  type        = string
  default     = "15d"
}

variable "prometheus_storage_size" {
  description = "Storage size for Prometheus data in GB"
  type        = number
  default     = 10
}

variable "grafana_cpu" {
  description = "CPU allocation for Grafana container"
  type        = number
  default     = 0.25
}

variable "grafana_memory" {
  description = "Memory allocation for Grafana container in GB"
  type        = number
  default     = 0.5
}

variable "grafana_storage_size" {
  description = "Storage size for Grafana data in GB"
  type        = number
  default     = 5
}

variable "grafana_admin_password" {
  description = "Admin password for Grafana dashboard"
  type        = string
  sensitive   = true
  default     = "latticedb-admin"
}

variable "insights_retention_days" {
  description = "Retention period for Application Insights data in days"
  type        = number
  default     = 90
}

variable "alert_email" {
  description = "Email address for monitoring alerts"
  type        = string
  default     = "admin@example.com"
}

variable "slack_webhook_url" {
  description = "Slack webhook URL for alerts (optional)"
  type        = string
  default     = ""
  sensitive   = true
}