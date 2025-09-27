output "resource_group_name" {
  description = "Name of the resource group"
  value       = azurerm_resource_group.main.name
}

output "container_app_url" {
  description = "URL of the deployed container app"
  value       = "https://${azurerm_container_app.latticedb.latest_revision_fqdn}"
}

output "application_url" {
  description = "Application URL (same as container app URL)"
  value       = "https://${azurerm_container_app.latticedb.latest_revision_fqdn}"
}

output "health_check_url" {
  description = "Health check endpoint URL"
  value       = "https://${azurerm_container_app.latticedb.latest_revision_fqdn}/health"
}

output "container_registry_login_server" {
  description = "Login server for the Azure Container Registry"
  value       = azurerm_container_registry.main.login_server
}

output "container_registry_name" {
  description = "Name of the Azure Container Registry"
  value       = azurerm_container_registry.main.name
}

output "container_app_name" {
  description = "Name of the container app"
  value       = azurerm_container_app.latticedb.name
}

output "container_app_environment_name" {
  description = "Name of the container app environment"
  value       = azurerm_container_app_environment.main.name
}

output "log_analytics_workspace_id" {
  description = "ID of the Log Analytics workspace"
  value       = azurerm_log_analytics_workspace.main.id
}

output "log_analytics_workspace_name" {
  description = "Name of the Log Analytics workspace"
  value       = azurerm_log_analytics_workspace.main.name
}

output "application_insights_connection_string" {
  description = "Connection string for Application Insights"
  value       = azurerm_application_insights.main.connection_string
  sensitive   = true
}

output "application_insights_instrumentation_key" {
  description = "Instrumentation key for Application Insights"
  value       = azurerm_application_insights.main.instrumentation_key
  sensitive   = true
}

output "key_vault_uri" {
  description = "URI of the Key Vault"
  value       = azurerm_key_vault.main.vault_uri
}

output "key_vault_name" {
  description = "Name of the Key Vault"
  value       = azurerm_key_vault.main.name
}

output "storage_account_name" {
  description = "Name of the storage account (if persistent storage is enabled)"
  value       = var.enable_persistent_storage ? azurerm_storage_account.main[0].name : ""
}

output "storage_account_primary_access_key" {
  description = "Primary access key for the storage account"
  value       = var.enable_persistent_storage ? azurerm_storage_account.main[0].primary_access_key : ""
  sensitive   = true
}

output "virtual_network_name" {
  description = "Name of the virtual network"
  value       = azurerm_virtual_network.main.name
}

output "virtual_network_id" {
  description = "ID of the virtual network"
  value       = azurerm_virtual_network.main.id
}

output "container_apps_subnet_id" {
  description = "ID of the container apps subnet"
  value       = azurerm_subnet.container_apps.id
}

output "user_assigned_identity_id" {
  description = "ID of the user assigned identity"
  value       = azurerm_user_assigned_identity.container_app.id
}

output "user_assigned_identity_principal_id" {
  description = "Principal ID of the user assigned identity"
  value       = azurerm_user_assigned_identity.container_app.principal_id
}

output "custom_domain_name" {
  description = "Custom domain name (if configured)"
  value       = var.domain_name != "" ? var.domain_name : ""
}

output "monitoring_enabled" {
  description = "Whether monitoring is enabled"
  value       = var.enable_monitoring
}

output "persistent_storage_enabled" {
  description = "Whether persistent storage is enabled"
  value       = var.enable_persistent_storage
}

output "deployment_summary" {
  description = "Summary of the deployment"
  value = {
    resource_group           = azurerm_resource_group.main.name
    location                = azurerm_resource_group.main.location
    application_url          = "https://${azurerm_container_app.latticedb.latest_revision_fqdn}"
    container_registry      = azurerm_container_registry.main.login_server
    monitoring_enabled      = var.enable_monitoring
    persistent_storage      = var.enable_persistent_storage
    min_replicas           = var.min_replicas
    max_replicas           = var.max_replicas
    environment            = var.environment
  }
}