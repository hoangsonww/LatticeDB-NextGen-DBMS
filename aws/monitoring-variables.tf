# Monitoring Configuration Variables

variable "enable_monitoring" {
  description = "Enable Prometheus and Grafana monitoring stack"
  type        = bool
  default     = true
}

variable "prometheus_cpu" {
  description = "CPU allocation for Prometheus container"
  type        = number
  default     = 512
}

variable "prometheus_memory" {
  description = "Memory allocation for Prometheus container in MB"
  type        = number
  default     = 1024
}

variable "prometheus_retention" {
  description = "Data retention period for Prometheus"
  type        = string
  default     = "15d"
}

variable "grafana_cpu" {
  description = "CPU allocation for Grafana container"
  type        = number
  default     = 256
}

variable "grafana_memory" {
  description = "Memory allocation for Grafana container in MB"
  type        = number
  default     = 512
}

variable "grafana_admin_password" {
  description = "Admin password for Grafana dashboard"
  type        = string
  sensitive   = true
  default     = "latticedb-admin"
}