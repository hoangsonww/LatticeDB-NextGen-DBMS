output "project_id" {
  description = "GCP Project ID"
  value       = var.project_id
}

output "region" {
  description = "GCP region"
  value       = var.gcp_region
}

output "cloud_run_service_name" {
  description = "Name of the Cloud Run service"
  value       = google_cloud_run_service.latticedb.name
}

output "cloud_run_service_url" {
  description = "URL of the Cloud Run service"
  value       = google_cloud_run_service.latticedb.status[0].url
}

output "application_url" {
  description = "Application URL (custom domain if configured, otherwise Cloud Run URL)"
  value       = var.custom_domain != "" ? "https://${var.custom_domain}" : google_cloud_run_service.latticedb.status[0].url
}

output "health_check_url" {
  description = "Health check endpoint URL"
  value       = "${var.custom_domain != "" ? "https://${var.custom_domain}" : google_cloud_run_service.latticedb.status[0].url}/health"
}

output "artifact_registry_repository" {
  description = "Artifact Registry repository name"
  value       = google_artifact_registry_repository.main.name
}

output "artifact_registry_url" {
  description = "Artifact Registry repository URL"
  value       = "${var.gcp_region}-docker.pkg.dev/${var.project_id}/${google_artifact_registry_repository.main.repository_id}"
}

output "service_account_email" {
  description = "Service account email for Cloud Run"
  value       = google_service_account.cloud_run.email
}

output "vpc_network_name" {
  description = "Name of the VPC network"
  value       = google_compute_network.main.name
}

output "vpc_network_id" {
  description = "ID of the VPC network"
  value       = google_compute_network.main.id
}

output "subnet_name" {
  description = "Name of the Cloud Run subnet"
  value       = google_compute_subnetwork.cloud_run.name
}

output "subnet_id" {
  description = "ID of the Cloud Run subnet"
  value       = google_compute_subnetwork.cloud_run.id
}

output "vpc_connector_name" {
  description = "Name of the VPC connector"
  value       = google_vpc_access_connector.main.name
}

output "vpc_connector_id" {
  description = "ID of the VPC connector"
  value       = google_vpc_access_connector.main.id
}

# Cloud SQL outputs (conditional)
output "cloud_sql_instance_name" {
  description = "Name of the Cloud SQL instance"
  value       = var.enable_cloud_sql ? google_sql_database_instance.main[0].name : ""
}

output "cloud_sql_instance_connection_name" {
  description = "Connection name of the Cloud SQL instance"
  value       = var.enable_cloud_sql ? google_sql_database_instance.main[0].connection_name : ""
}

output "cloud_sql_private_ip" {
  description = "Private IP address of the Cloud SQL instance"
  value       = var.enable_cloud_sql ? google_sql_database_instance.main[0].private_ip_address : ""
}

output "database_name" {
  description = "Name of the database"
  value       = var.enable_cloud_sql ? google_sql_database.latticedb[0].name : ""
}

output "database_user" {
  description = "Database user name"
  value       = var.enable_cloud_sql ? google_sql_user.latticedb[0].name : ""
}

output "database_password_secret_name" {
  description = "Name of the Secret Manager secret containing database password"
  value       = var.enable_cloud_sql && var.enable_secret_manager ? google_secret_manager_secret.db_password[0].secret_id : ""
}

# Cloud Storage outputs (conditional)
output "storage_bucket_name" {
  description = "Name of the Cloud Storage bucket"
  value       = var.enable_cloud_storage ? google_storage_bucket.data[0].name : ""
}

output "storage_bucket_url" {
  description = "URL of the Cloud Storage bucket"
  value       = var.enable_cloud_storage ? google_storage_bucket.data[0].url : ""
}

# Load Balancer outputs (conditional)
output "load_balancer_ip" {
  description = "IP address of the load balancer"
  value       = var.custom_domain != "" ? google_compute_global_address.lb[0].address : ""
}

output "ssl_certificate_name" {
  description = "Name of the SSL certificate"
  value       = var.custom_domain != "" ? google_compute_managed_ssl_certificate.main[0].name : ""
}

output "custom_domain" {
  description = "Custom domain name (if configured)"
  value       = var.custom_domain
}

# Monitoring outputs (conditional)
output "monitoring_dashboard_id" {
  description = "ID of the monitoring dashboard"
  value       = var.enable_monitoring ? google_monitoring_dashboard.main[0].id : ""
}

output "monitoring_enabled" {
  description = "Whether monitoring is enabled"
  value       = var.enable_monitoring
}

# Security outputs
output "secret_manager_enabled" {
  description = "Whether Secret Manager is enabled"
  value       = var.enable_secret_manager
}

output "kms_key_name" {
  description = "KMS key name used for encryption"
  value       = var.kms_key_name
}

# Configuration summary
output "deployment_summary" {
  description = "Summary of the deployment configuration"
  value = {
    project_id          = var.project_id
    region             = var.gcp_region
    environment        = var.environment
    application_url    = var.custom_domain != "" ? "https://${var.custom_domain}" : google_cloud_run_service.latticedb.status[0].url
    cloud_run_service  = google_cloud_run_service.latticedb.name
    artifact_registry  = "${var.gcp_region}-docker.pkg.dev/${var.project_id}/${google_artifact_registry_repository.main.repository_id}"
    min_instances      = var.min_instances
    max_instances      = var.max_instances
    cpu_limit          = var.cpu_limit
    memory_limit       = var.memory_limit
    cloud_sql_enabled  = var.enable_cloud_sql
    storage_enabled    = var.enable_cloud_storage
    monitoring_enabled = var.enable_monitoring
    custom_domain      = var.custom_domain
    cdn_enabled        = var.enable_cdn
  }
}

# Environment-specific outputs
output "environment_info" {
  description = "Environment-specific information"
  value = {
    is_production = var.environment == "production"
    deletion_protection = var.deletion_protection
    backup_retention_days = var.backup_retention_days
    point_in_time_recovery = var.enable_point_in_time_recovery
  }
}

# Network configuration outputs
output "network_config" {
  description = "Network configuration details"
  value = {
    vpc_network        = google_compute_network.main.name
    subnet_cidr        = var.subnet_cidr
    vpc_connector_cidr = var.vpc_connector_cidr
    cloud_sql_cidr     = var.cloud_sql_cidr
  }
}

# Resource names for management commands
output "resource_names" {
  description = "Resource names for management commands"
  value = {
    cloud_run_service  = google_cloud_run_service.latticedb.name
    service_account    = google_service_account.cloud_run.email
    vpc_network        = google_compute_network.main.name
    subnet             = google_compute_subnetwork.cloud_run.name
    artifact_registry  = google_artifact_registry_repository.main.repository_id
    cloud_sql_instance = var.enable_cloud_sql ? google_sql_database_instance.main[0].name : ""
    storage_bucket     = var.enable_cloud_storage ? google_storage_bucket.data[0].name : ""
  }
}