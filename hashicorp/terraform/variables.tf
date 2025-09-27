variable "project_name" {
  description = "Name of the project"
  type        = string
  default     = "latticedb"
}

variable "environment" {
  description = "Environment (e.g., production, staging, development)"
  type        = string
  default     = "production"
  validation {
    condition     = contains(["production", "staging", "development"], var.environment)
    error_message = "Environment must be one of: production, staging, development."
  }
}

# HashiCorp Stack Configuration
variable "consul_address" {
  description = "Consul server address"
  type        = string
  default     = "http://127.0.0.1:8500"
}

variable "consul_datacenter" {
  description = "Consul datacenter name"
  type        = string
  default     = "dc1"
}

variable "consul_token" {
  description = "Consul ACL token"
  type        = string
  default     = ""
  sensitive   = true
}

variable "vault_address" {
  description = "Vault server address"
  type        = string
  default     = "http://127.0.0.1:8200"
}

variable "vault_token" {
  description = "Vault authentication token"
  type        = string
  default     = ""
  sensitive   = true
}

variable "vault_namespace" {
  description = "Vault namespace (Vault Enterprise only)"
  type        = string
  default     = ""
}

variable "nomad_address" {
  description = "Nomad server address"
  type        = string
  default     = "http://127.0.0.1:4646"
}

variable "nomad_region" {
  description = "Nomad region"
  type        = string
  default     = "global"
}

variable "nomad_token" {
  description = "Nomad authentication token"
  type        = string
  default     = ""
  sensitive   = true
}

# Application Configuration
variable "container_image" {
  description = "Container image for LatticeDB"
  type        = string
  default     = "latticedb/latticedb"
}

variable "image_tag" {
  description = "Container image tag"
  type        = string
  default     = "latest"
}

variable "instance_count" {
  description = "Number of LatticeDB instances to run"
  type        = number
  default     = 3
  validation {
    condition     = var.instance_count >= 1 && var.instance_count <= 20
    error_message = "Instance count must be between 1 and 20."
  }
}

variable "cpu_limit" {
  description = "CPU limit in MHz"
  type        = number
  default     = 1000
}

variable "memory_limit" {
  description = "Memory limit in MB"
  type        = number
  default     = 2048
}

variable "http_port" {
  description = "HTTP API port"
  type        = number
  default     = 8080
}

variable "sql_port" {
  description = "SQL interface port"
  type        = number
  default     = 5432
}

variable "health_port" {
  description = "Health check port"
  type        = number
  default     = 8081
}

variable "log_level" {
  description = "Application log level"
  type        = string
  default     = "info"
  validation {
    condition     = contains(["debug", "info", "warn", "error"], var.log_level)
    error_message = "Log level must be one of: debug, info, warn, error."
  }
}

variable "max_connections" {
  description = "Maximum number of concurrent connections"
  type        = number
  default     = 1000
}

# Security Configuration
variable "enable_tls" {
  description = "Enable TLS/SSL encryption"
  type        = bool
  default     = true
}

variable "enable_consul_acl" {
  description = "Enable Consul ACL system"
  type        = bool
  default     = true
}

variable "enable_consul_connect" {
  description = "Enable Consul Connect service mesh"
  type        = bool
  default     = true
}

variable "enable_vault_pki" {
  description = "Enable Vault PKI for certificate management"
  type        = bool
  default     = true
}

# Storage Configuration
variable "enable_persistent_storage" {
  description = "Enable persistent storage with CSI"
  type        = bool
  default     = true
}

variable "csi_plugin_id" {
  description = "CSI plugin ID for persistent storage"
  type        = string
  default     = "aws-ebs0"
}

variable "storage_size_min" {
  description = "Minimum storage size in GiB"
  type        = number
  default     = 10
}

variable "storage_size_max" {
  description = "Maximum storage size in GiB"
  type        = number
  default     = 100
}

variable "availability_zones" {
  description = "List of availability zones for storage placement"
  type        = list(string)
  default     = ["us-west-2a", "us-west-2b", "us-west-2c"]
}

# Database Configuration
variable "db_connection_url" {
  description = "Database connection URL for dynamic credentials"
  type        = string
  default     = "postgresql://{{username}}:{{password}}@localhost:5432/latticedb?sslmode=disable"
}

variable "db_username" {
  description = "Database username for Vault connection"
  type        = string
  default     = "vault"
}

variable "db_password" {
  description = "Database password (leave empty to generate random password)"
  type        = string
  default     = ""
  sensitive   = true
}

variable "admin_password" {
  description = "Admin password (leave empty to generate random password)"
  type        = string
  default     = ""
  sensitive   = true
}

# Backup Configuration
variable "enable_backup" {
  description = "Enable automated backups"
  type        = bool
  default     = true
}

variable "backup_interval" {
  description = "Backup interval (e.g., 1h, 30m)"
  type        = string
  default     = "1h"
  validation {
    condition     = can(regex("^[0-9]+(h|m|s)$", var.backup_interval))
    error_message = "Backup interval must be in format like '1h', '30m', or '60s'."
  }
}

variable "backup_retention_days" {
  description = "Number of days to retain backups"
  type        = number
  default     = 7
}

# Networking Configuration
variable "enable_ingress_gateway" {
  description = "Enable Consul ingress gateway"
  type        = bool
  default     = false
}

variable "ingress_hosts" {
  description = "Hosts for ingress gateway"
  type        = list(string)
  default     = ["latticedb.local", "*.latticedb.local"]
}

# Monitoring and Observability
variable "enable_monitoring" {
  description = "Enable monitoring and metrics collection"
  type        = bool
  default     = true
}

variable "enable_tracing" {
  description = "Enable distributed tracing"
  type        = bool
  default     = false
}

variable "jaeger_endpoint" {
  description = "Jaeger collector endpoint for tracing"
  type        = string
  default     = "http://jaeger-collector:14268/api/traces"
}

variable "prometheus_endpoint" {
  description = "Prometheus endpoint for metrics scraping"
  type        = string
  default     = "http://prometheus:9090"
}

# Deployment Configuration
variable "enable_canary_deployments" {
  description = "Enable canary deployment strategy"
  type        = bool
  default     = false
}

variable "canary_weight" {
  description = "Percentage of traffic to route to canary version"
  type        = number
  default     = 10
  validation {
    condition     = var.canary_weight >= 0 && var.canary_weight <= 100
    error_message = "Canary weight must be between 0 and 100."
  }
}

variable "update_strategy" {
  description = "Update strategy for deployments"
  type        = string
  default     = "rolling"
  validation {
    condition     = contains(["rolling", "blue-green", "canary"], var.update_strategy)
    error_message = "Update strategy must be one of: rolling, blue-green, canary."
  }
}

variable "max_parallel_updates" {
  description = "Maximum number of parallel updates during deployment"
  type        = number
  default     = 2
}

variable "min_healthy_time" {
  description = "Minimum time an allocation must be healthy before considering it stable"
  type        = string
  default     = "30s"
}

variable "healthy_deadline" {
  description = "Deadline for an allocation to become healthy"
  type        = string
  default     = "5m"
}

variable "auto_revert" {
  description = "Enable automatic revert on failed deployments"
  type        = bool
  default     = true
}

# Resource Constraints
variable "node_class" {
  description = "Nomad node class for scheduling"
  type        = string
  default     = "compute"
}

variable "node_pool" {
  description = "Nomad node pool for scheduling"
  type        = string
  default     = "default"
}

variable "constraint_attributes" {
  description = "Additional node constraints for scheduling"
  type        = map(string)
  default     = {}
}

# High Availability Configuration
variable "enable_anti_affinity" {
  description = "Enable anti-affinity to spread instances across nodes"
  type        = bool
  default     = true
}

variable "enable_leader_election" {
  description = "Enable leader election using Consul"
  type        = bool
  default     = true
}

variable "cluster_join_method" {
  description = "Method for cluster joining (consul, static, auto)"
  type        = string
  default     = "consul"
  validation {
    condition     = contains(["consul", "static", "auto"], var.cluster_join_method)
    error_message = "Cluster join method must be one of: consul, static, auto."
  }
}

# Additional Tags
variable "tags" {
  description = "Additional tags to apply to all resources"
  type        = map(string)
  default     = {}
}

# External Integrations
variable "enable_external_dns" {
  description = "Enable external DNS integration"
  type        = bool
  default     = false
}

variable "dns_zone" {
  description = "DNS zone for external DNS records"
  type        = string
  default     = "latticedb.local"
}

variable "enable_service_mesh_observability" {
  description = "Enable service mesh observability features"
  type        = bool
  default     = true
}

variable "mesh_gateway_mode" {
  description = "Mesh gateway mode (local, remote, none)"
  type        = string
  default     = "local"
  validation {
    condition     = contains(["local", "remote", "none"], var.mesh_gateway_mode)
    error_message = "Mesh gateway mode must be one of: local, remote, none."
  }
}

# Development and Testing
variable "enable_debug_mode" {
  description = "Enable debug mode for development"
  type        = bool
  default     = false
}

variable "enable_profiling" {
  description = "Enable application profiling endpoints"
  type        = bool
  default     = false
}

variable "test_data_enabled" {
  description = "Enable test data population"
  type        = bool
  default     = false
}

# Compliance and Security
variable "enable_audit_logging" {
  description = "Enable comprehensive audit logging"
  type        = bool
  default     = true
}

variable "compliance_mode" {
  description = "Compliance mode (none, basic, strict)"
  type        = string
  default     = "basic"
  validation {
    condition     = contains(["none", "basic", "strict"], var.compliance_mode)
    error_message = "Compliance mode must be one of: none, basic, strict."
  }
}

variable "enable_encryption_at_rest" {
  description = "Enable encryption at rest for storage"
  type        = bool
  default     = true
}

variable "enable_encryption_in_transit" {
  description = "Enable encryption in transit"
  type        = bool
  default     = true
}