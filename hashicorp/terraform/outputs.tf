output "cluster_name" {
  description = "Name of the LatticeDB cluster"
  value       = local.cluster_name
}

output "environment" {
  description = "Environment name"
  value       = var.environment
}

output "project_name" {
  description = "Project name"
  value       = var.project_name
}

# Consul Outputs
output "consul_address" {
  description = "Consul server address"
  value       = var.consul_address
}

output "consul_datacenter" {
  description = "Consul datacenter"
  value       = var.consul_datacenter
}

output "consul_service_name" {
  description = "Consul service name for LatticeDB"
  value       = "latticedb"
}

output "consul_acl_token" {
  description = "Consul ACL token for LatticeDB service"
  value       = var.enable_consul_acl ? consul_acl_token.latticedb[0].secret_id : ""
  sensitive   = true
}

output "consul_connect_enabled" {
  description = "Whether Consul Connect is enabled"
  value       = var.enable_consul_connect
}

# Vault Outputs
output "vault_address" {
  description = "Vault server address"
  value       = var.vault_address
}

output "vault_kv_path" {
  description = "Vault KV path for LatticeDB secrets"
  value       = "secret/latticedb/${var.environment}"
}

output "vault_pki_role" {
  description = "Vault PKI role for LatticeDB certificates"
  value       = vault_pki_secret_backend_role.latticedb.name
}

output "vault_database_role" {
  description = "Vault database role for dynamic credentials"
  value       = vault_database_secret_backend_role.latticedb.name
}

output "vault_transit_key" {
  description = "Vault transit encryption key name"
  value       = vault_transit_secret_backend_key.latticedb.name
}

output "vault_approle_role_id" {
  description = "Vault AppRole role ID"
  value       = vault_approle_auth_backend_role.latticedb.role_id
  sensitive   = true
}

output "vault_approle_secret_id" {
  description = "Vault AppRole secret ID"
  value       = vault_approle_auth_backend_role_secret_id.latticedb.secret_id
  sensitive   = true
}

# Nomad Outputs
output "nomad_address" {
  description = "Nomad server address"
  value       = var.nomad_address
}

output "nomad_region" {
  description = "Nomad region"
  value       = var.nomad_region
}

output "nomad_job_name" {
  description = "Nomad job name"
  value       = nomad_job.latticedb.name
}

output "nomad_job_id" {
  description = "Nomad job ID"
  value       = nomad_job.latticedb.id
}

output "nomad_namespace" {
  description = "Nomad namespace"
  value       = nomad_job.latticedb.namespace
}

# Application Configuration Outputs
output "application_ports" {
  description = "Application port configuration"
  value = {
    http   = var.http_port
    sql    = var.sql_port
    health = var.health_port
  }
}

output "instance_count" {
  description = "Number of running instances"
  value       = var.instance_count
}

output "container_image" {
  description = "Container image and tag"
  value       = "${var.container_image}:${var.image_tag}"
}

output "cpu_limit" {
  description = "CPU limit in MHz"
  value       = var.cpu_limit
}

output "memory_limit" {
  description = "Memory limit in MB"
  value       = var.memory_limit
}

# Security Outputs
output "tls_enabled" {
  description = "Whether TLS is enabled"
  value       = var.enable_tls
}

output "acl_enabled" {
  description = "Whether ACL is enabled"
  value       = var.enable_consul_acl
}

output "service_mesh_enabled" {
  description = "Whether service mesh is enabled"
  value       = var.enable_consul_connect
}

# Storage Outputs
output "persistent_storage_enabled" {
  description = "Whether persistent storage is enabled"
  value       = var.enable_persistent_storage
}

output "csi_volume_id" {
  description = "CSI volume ID for persistent storage"
  value       = var.enable_persistent_storage ? nomad_csi_volume.latticedb_data[0].volume_id : ""
}

output "storage_size" {
  description = "Storage size configuration"
  value = {
    min = var.storage_size_min
    max = var.storage_size_max
  }
}

# Database Outputs
output "database_connection_configured" {
  description = "Whether database connection is configured"
  value       = var.db_connection_url != ""
}

output "vault_database_path" {
  description = "Vault database secrets path"
  value       = vault_mount.database.path
}

# Backup Outputs
output "backup_enabled" {
  description = "Whether backups are enabled"
  value       = var.enable_backup
}

output "backup_interval" {
  description = "Backup interval"
  value       = var.backup_interval
}

output "backup_retention_days" {
  description = "Backup retention period in days"
  value       = var.backup_retention_days
}

# Networking Outputs
output "ingress_gateway_enabled" {
  description = "Whether ingress gateway is enabled"
  value       = var.enable_ingress_gateway
}

output "ingress_hosts" {
  description = "Ingress gateway hosts"
  value       = var.ingress_hosts
}

output "dns_zone" {
  description = "DNS zone for external DNS"
  value       = var.dns_zone
}

# Monitoring Outputs
output "monitoring_enabled" {
  description = "Whether monitoring is enabled"
  value       = var.enable_monitoring
}

output "tracing_enabled" {
  description = "Whether distributed tracing is enabled"
  value       = var.enable_tracing
}

output "prometheus_endpoint" {
  description = "Prometheus endpoint for metrics"
  value       = var.prometheus_endpoint
}

output "jaeger_endpoint" {
  description = "Jaeger endpoint for tracing"
  value       = var.jaeger_endpoint
}

# Deployment Configuration Outputs
output "deployment_strategy" {
  description = "Deployment strategy configuration"
  value = {
    update_strategy       = var.update_strategy
    canary_enabled       = var.enable_canary_deployments
    canary_weight        = var.canary_weight
    max_parallel_updates = var.max_parallel_updates
    auto_revert          = var.auto_revert
  }
}

output "high_availability_config" {
  description = "High availability configuration"
  value = {
    anti_affinity_enabled = var.enable_anti_affinity
    leader_election       = var.enable_leader_election
    cluster_join_method   = var.cluster_join_method
  }
}

# Resource Information
output "node_constraints" {
  description = "Node constraints for scheduling"
  value = {
    node_class  = var.node_class
    node_pool   = var.node_pool
    attributes  = var.constraint_attributes
  }
}

# Service Mesh Configuration
output "service_mesh_config" {
  description = "Service mesh configuration details"
  value = {
    connect_enabled       = var.enable_consul_connect
    mesh_gateway_mode    = var.mesh_gateway_mode
    observability_enabled = var.enable_service_mesh_observability
  }
}

# Compliance and Security
output "compliance_config" {
  description = "Compliance and security configuration"
  value = {
    compliance_mode           = var.compliance_mode
    audit_logging_enabled     = var.enable_audit_logging
    encryption_at_rest       = var.enable_encryption_at_rest
    encryption_in_transit    = var.enable_encryption_in_transit
  }
}

# Development and Testing
output "development_config" {
  description = "Development and testing configuration"
  value = {
    debug_mode_enabled = var.enable_debug_mode
    profiling_enabled  = var.enable_profiling
    test_data_enabled  = var.test_data_enabled
  }
}

# Connection Information
output "service_endpoints" {
  description = "Service endpoints for connecting to LatticeDB"
  value = {
    http_api       = "http://latticedb.service.consul:${var.http_port}"
    sql_interface  = "postgresql://latticedb.service.consul:${var.sql_port}/latticedb"
    health_check   = "http://latticedb.service.consul:${var.health_port}/health"
    metrics        = "http://latticedb.service.consul:${var.http_port}/metrics"
  }
}

# Management Commands
output "management_commands" {
  description = "Useful management commands"
  value = {
    nomad_status      = "nomad job status ${nomad_job.latticedb.name}"
    nomad_logs        = "nomad logs -f ${nomad_job.latticedb.name}"
    nomad_scale       = "nomad job scale ${nomad_job.latticedb.name} <count>"
    consul_services   = "consul catalog services -tags"
    consul_health     = "consul health service latticedb"
    vault_secrets     = "vault kv get secret/latticedb/${var.environment}/config"
    vault_cert        = "vault write pki_int/issue/latticedb common_name=latticedb.service.consul"
  }
}

# Summary Information
output "deployment_summary" {
  description = "Summary of the deployment configuration"
  value = {
    cluster_name     = local.cluster_name
    environment      = var.environment
    instances        = var.instance_count
    consul_dc        = var.consul_datacenter
    nomad_region     = var.nomad_region
    vault_enabled    = true
    connect_enabled  = var.enable_consul_connect
    tls_enabled      = var.enable_tls
    storage_enabled  = var.enable_persistent_storage
    backup_enabled   = var.enable_backup
    monitoring_enabled = var.enable_monitoring
  }
}

# Infrastructure Status
output "infrastructure_ready" {
  description = "Whether the infrastructure is ready for deployment"
  value = {
    vault_configured     = true
    consul_configured    = true
    nomad_job_deployed   = true
    secrets_created      = true
    certificates_ready   = var.enable_vault_pki
    storage_provisioned  = var.enable_persistent_storage
  }
}