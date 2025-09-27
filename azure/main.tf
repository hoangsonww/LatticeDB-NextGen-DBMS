terraform {
  required_version = ">= 1.0"
  required_providers {
    azurerm = {
      source  = "hashicorp/azurerm"
      version = "~> 3.0"
    }
    azuread = {
      source  = "hashicorp/azuread"
      version = "~> 2.0"
    }
  }
  backend "azurerm" {
    # Configure backend via terraform init -backend-config
    # resource_group_name  = "terraform-state-rg"
    # storage_account_name = "terraformstatestorage"
    # container_name       = "tfstate"
    # key                  = "latticedb/terraform.tfstate"
  }
}

provider "azurerm" {
  features {
    key_vault {
      purge_soft_delete_on_destroy    = true
      recover_soft_deleted_key_vaults = true
    }
    resource_group {
      prevent_deletion_if_contains_resources = false
    }
  }
}

# Data sources
data "azurerm_client_config" "current" {}

# Local values
locals {
  cluster_name = "${var.project_name}-${var.environment}"
  common_tags = merge(var.tags, {
    Project     = var.project_name
    Environment = var.environment
    ManagedBy   = "Terraform"
  })
}

# Resource Group
resource "azurerm_resource_group" "main" {
  name     = "${local.cluster_name}-rg"
  location = var.azure_location

  tags = local.common_tags
}

# Virtual Network
resource "azurerm_virtual_network" "main" {
  name                = "${local.cluster_name}-vnet"
  address_space       = [var.vnet_cidr]
  location            = azurerm_resource_group.main.location
  resource_group_name = azurerm_resource_group.main.name

  tags = local.common_tags
}

# Subnets
resource "azurerm_subnet" "container_apps" {
  name                 = "${local.cluster_name}-container-apps-subnet"
  resource_group_name  = azurerm_resource_group.main.name
  virtual_network_name = azurerm_virtual_network.main.name
  address_prefixes     = [cidrsubnet(var.vnet_cidr, 8, 1)]

  delegation {
    name = "container-apps-delegation"
    service_delegation {
      name    = "Microsoft.App/environments"
      actions = ["Microsoft.Network/virtualNetworks/subnets/join/action"]
    }
  }
}

resource "azurerm_subnet" "private_endpoints" {
  name                 = "${local.cluster_name}-private-endpoints-subnet"
  resource_group_name  = azurerm_resource_group.main.name
  virtual_network_name = azurerm_virtual_network.main.name
  address_prefixes     = [cidrsubnet(var.vnet_cidr, 8, 2)]

  private_endpoint_network_policies_enabled = true
}

# Network Security Groups
resource "azurerm_network_security_group" "container_apps" {
  name                = "${local.cluster_name}-container-apps-nsg"
  location            = azurerm_resource_group.main.location
  resource_group_name = azurerm_resource_group.main.name

  security_rule {
    name                       = "HTTP"
    priority                   = 1001
    direction                  = "Inbound"
    access                     = "Allow"
    protocol                   = "Tcp"
    source_port_range          = "*"
    destination_port_range     = "8080"
    source_address_prefix      = "*"
    destination_address_prefix = "*"
  }

  security_rule {
    name                       = "SQL"
    priority                   = 1002
    direction                  = "Inbound"
    access                     = "Allow"
    protocol                   = "Tcp"
    source_port_range          = "*"
    destination_port_range     = "5432"
    source_address_prefix      = "*"
    destination_address_prefix = "*"
  }

  tags = local.common_tags
}

resource "azurerm_subnet_network_security_group_association" "container_apps" {
  subnet_id                 = azurerm_subnet.container_apps.id
  network_security_group_id = azurerm_network_security_group.container_apps.id
}

# Container Registry
resource "azurerm_container_registry" "main" {
  name                = replace("${var.project_name}${var.environment}acr", "-", "")
  resource_group_name = azurerm_resource_group.main.name
  location            = azurerm_resource_group.main.location
  sku                 = var.acr_sku
  admin_enabled       = true

  identity {
    type = "SystemAssigned"
  }

  encryption {
    enabled = true
  }

  tags = local.common_tags
}

# Log Analytics Workspace
resource "azurerm_log_analytics_workspace" "main" {
  name                = "${local.cluster_name}-logs"
  location            = azurerm_resource_group.main.location
  resource_group_name = azurerm_resource_group.main.name
  sku                 = "PerGB2018"
  retention_in_days   = 30

  tags = local.common_tags
}

# Application Insights
resource "azurerm_application_insights" "main" {
  name                = "${local.cluster_name}-appinsights"
  location            = azurerm_resource_group.main.location
  resource_group_name = azurerm_resource_group.main.name
  workspace_id        = azurerm_log_analytics_workspace.main.id
  application_type    = "web"

  tags = local.common_tags
}

# Key Vault
resource "azurerm_key_vault" "main" {
  name                = "${replace(local.cluster_name, "-", "")}kv"
  location            = azurerm_resource_group.main.location
  resource_group_name = azurerm_resource_group.main.name
  tenant_id           = data.azurerm_client_config.current.tenant_id
  sku_name            = "standard"

  access_policy {
    tenant_id = data.azurerm_client_config.current.tenant_id
    object_id = data.azurerm_client_config.current.object_id

    key_permissions = [
      "Backup", "Create", "Decrypt", "Delete", "Encrypt", "Get", "Import",
      "List", "Purge", "Recover", "Restore", "Sign", "UnwrapKey", "Update",
      "Verify", "WrapKey", "Release", "Rotate", "GetRotationPolicy",
      "SetRotationPolicy"
    ]

    secret_permissions = [
      "Backup", "Delete", "Get", "List", "Purge", "Recover", "Restore", "Set"
    ]

    certificate_permissions = [
      "Backup", "Create", "Delete", "DeleteIssuers", "Get", "GetIssuers",
      "Import", "List", "ListIssuers", "ManageContacts", "ManageIssuers",
      "Purge", "Recover", "Restore", "SetIssuers", "Update"
    ]
  }

  network_acls {
    bypass         = "AzureServices"
    default_action = "Allow"
  }

  tags = local.common_tags
}

# Storage Account for persistent data
resource "azurerm_storage_account" "main" {
  count                    = var.enable_persistent_storage ? 1 : 0
  name                     = replace("${var.project_name}${var.environment}sa", "-", "")
  resource_group_name      = azurerm_resource_group.main.name
  location                 = azurerm_resource_group.main.location
  account_tier             = "Standard"
  account_replication_type = var.storage_replication_type

  blob_properties {
    delete_retention_policy {
      days = 7
    }
  }

  tags = local.common_tags
}

resource "azurerm_storage_share" "latticedb" {
  count                = var.enable_persistent_storage ? 1 : 0
  name                 = "latticedb-data"
  storage_account_name = azurerm_storage_account.main[0].name
  quota                = 100
}

# User Assigned Identity for Container App
resource "azurerm_user_assigned_identity" "container_app" {
  name                = "${local.cluster_name}-identity"
  location            = azurerm_resource_group.main.location
  resource_group_name = azurerm_resource_group.main.name

  tags = local.common_tags
}

# Role assignment for ACR access
resource "azurerm_role_assignment" "acr_pull" {
  scope                = azurerm_container_registry.main.id
  role_definition_name = "AcrPull"
  principal_id         = azurerm_user_assigned_identity.container_app.principal_id
}

# Key Vault access policy for Container App identity
resource "azurerm_key_vault_access_policy" "container_app" {
  key_vault_id = azurerm_key_vault.main.id
  tenant_id    = data.azurerm_client_config.current.tenant_id
  object_id    = azurerm_user_assigned_identity.container_app.principal_id

  secret_permissions = [
    "Get", "List"
  ]
}

# Container Apps Environment
resource "azurerm_container_app_environment" "main" {
  name                       = "${local.cluster_name}-env"
  location                   = azurerm_resource_group.main.location
  resource_group_name        = azurerm_resource_group.main.name
  log_analytics_workspace_id = azurerm_log_analytics_workspace.main.id
  infrastructure_subnet_id   = azurerm_subnet.container_apps.id

  tags = local.common_tags
}

# Container App
resource "azurerm_container_app" "latticedb" {
  name                         = "${local.cluster_name}-app"
  container_app_environment_id = azurerm_container_app_environment.main.id
  resource_group_name          = azurerm_resource_group.main.name
  revision_mode                = "Single"

  identity {
    type         = "UserAssigned"
    identity_ids = [azurerm_user_assigned_identity.container_app.id]
  }

  registry {
    server   = azurerm_container_registry.main.login_server
    identity = azurerm_user_assigned_identity.container_app.id
  }

  ingress {
    external_enabled = true
    target_port      = 8080
    allow_insecure_connections = false

    traffic_weight {
      percentage      = 100
      latest_revision = true
    }
  }

  template {
    container {
      name   = "latticedb"
      image  = "${azurerm_container_registry.main.login_server}/${var.project_name}:${var.image_tag}"
      cpu    = var.container_cpu
      memory = var.container_memory

      env {
        name  = "LATTICEDB_ENV"
        value = var.environment
      }

      env {
        name  = "LATTICEDB_LOG_LEVEL"
        value = var.log_level
      }

      env {
        name  = "APPLICATIONINSIGHTS_CONNECTION_STRING"
        value = azurerm_application_insights.main.connection_string
      }

      dynamic "volume_mounts" {
        for_each = var.enable_persistent_storage ? [1] : []
        content {
          name = "latticedb-data"
          path = "/var/lib/latticedb"
        }
      }

      liveness_probe {
        transport = "HTTP"
        port      = 8080
        path      = "/health"
      }

      readiness_probe {
        transport = "HTTP"
        port      = 8080
        path      = "/ready"
      }
    }

    min_replicas = var.min_replicas
    max_replicas = var.max_replicas

    dynamic "volume" {
      for_each = var.enable_persistent_storage ? [1] : []
      content {
        name         = "latticedb-data"
        storage_type = "AzureFile"
        storage_name = azurerm_container_app_environment_storage.latticedb[0].name
      }
    }
  }

  tags = local.common_tags

  depends_on = [
    azurerm_role_assignment.acr_pull,
    azurerm_key_vault_access_policy.container_app
  ]
}

# Container App Environment Storage (for persistent data)
resource "azurerm_container_app_environment_storage" "latticedb" {
  count                        = var.enable_persistent_storage ? 1 : 0
  name                         = "latticedb-storage"
  container_app_environment_id = azurerm_container_app_environment.main.id
  account_name                 = azurerm_storage_account.main[0].name
  share_name                   = azurerm_storage_share.latticedb[0].name
  access_key                   = azurerm_storage_account.main[0].primary_access_key
  access_mode                  = "ReadWrite"
}

# Monitoring Alert Rules
resource "azurerm_monitor_metric_alert" "cpu_usage" {
  count               = var.enable_monitoring ? 1 : 0
  name                = "${local.cluster_name}-cpu-alert"
  resource_group_name = azurerm_resource_group.main.name
  scopes              = [azurerm_container_app.latticedb.id]
  description         = "Action will be triggered when CPU usage is greater than 80%"

  criteria {
    metric_namespace = "Microsoft.App/containerApps"
    metric_name      = "CpuPercentage"
    aggregation      = "Average"
    operator         = "GreaterThan"
    threshold        = 80
  }

  action {
    action_group_id = azurerm_monitor_action_group.main[0].id
  }

  tags = local.common_tags
}

resource "azurerm_monitor_metric_alert" "memory_usage" {
  count               = var.enable_monitoring ? 1 : 0
  name                = "${local.cluster_name}-memory-alert"
  resource_group_name = azurerm_resource_group.main.name
  scopes              = [azurerm_container_app.latticedb.id]
  description         = "Action will be triggered when memory usage is greater than 85%"

  criteria {
    metric_namespace = "Microsoft.App/containerApps"
    metric_name      = "MemoryPercentage"
    aggregation      = "Average"
    operator         = "GreaterThan"
    threshold        = 85
  }

  action {
    action_group_id = azurerm_monitor_action_group.main[0].id
  }

  tags = local.common_tags
}

# Action Group for alerts
resource "azurerm_monitor_action_group" "main" {
  count               = var.enable_monitoring ? 1 : 0
  name                = "${local.cluster_name}-action-group"
  resource_group_name = azurerm_resource_group.main.name
  short_name          = "latticedb"

  email_receiver {
    name          = "admin"
    email_address = var.notification_email
  }

  tags = local.common_tags
}

# Custom Domain (optional)
resource "azurerm_container_app_custom_domain" "main" {
  count            = var.domain_name != "" ? 1 : 0
  name             = var.domain_name
  container_app_id = azurerm_container_app.latticedb.id
  container_app_environment_certificate_id = var.certificate_id
  binding_type     = "SniEnabled"
}