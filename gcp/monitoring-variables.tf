# Monitoring Configuration Variables for GCP

variable "enable_monitoring" {
  description = "Enable Prometheus and Grafana monitoring stack"
  type        = bool
  default     = true
}

variable "prometheus_cpu" {
  description = "CPU allocation for Prometheus container in millicores"
  type        = number
  default     = 1000
}

variable "prometheus_memory" {
  description = "Memory allocation for Prometheus container in MB"
  type        = number
  default     = 2048
}

variable "prometheus_retention" {
  description = "Data retention period for Prometheus"
  type        = string
  default     = "15d"
}

variable "grafana_cpu" {
  description = "CPU allocation for Grafana container in millicores"
  type        = number
  default     = 500
}

variable "grafana_memory" {
  description = "Memory allocation for Grafana container in MB"
  type        = number
  default     = 1024
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

variable "notification_channels" {
  description = "List of notification channel IDs for Cloud Monitoring alerts"
  type        = list(string)
  default     = []
}

variable "enable_uptime_checks" {
  description = "Enable uptime checks for the LatticeDB service"
  type        = bool
  default     = true
}