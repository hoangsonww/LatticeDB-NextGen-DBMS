# Monitoring Infrastructure - Prometheus & Grafana for Azure

# Container Apps for Monitoring
resource "azurerm_container_app" "prometheus" {
  count                        = var.enable_monitoring ? 1 : 0
  name                         = "${var.project_name}-prometheus-${var.environment}"
  container_app_environment_id = azurerm_container_app_environment.main.id
  resource_group_name          = azurerm_resource_group.main.name
  revision_mode                = "Single"

  template {
    min_replicas = 1
    max_replicas = 1

    container {
      name   = "prometheus"
      image  = "prom/prometheus:v2.40.0"
      cpu    = var.prometheus_cpu
      memory = "${var.prometheus_memory}Gi"

      args = [
        "--config.file=/etc/prometheus/prometheus.yml",
        "--storage.tsdb.path=/prometheus",
        "--storage.tsdb.retention.time=${var.prometheus_retention}",
        "--web.console.libraries=/etc/prometheus/console_libraries",
        "--web.console.templates=/etc/prometheus/consoles",
        "--web.enable-lifecycle"
      ]

      env {
        name  = "LATTICEDB_ENVIRONMENT"
        value = var.environment
      }

      volume_mounts {
        name = "prometheus-config"
        path = "/etc/prometheus"
      }

      volume_mounts {
        name = "prometheus-data"
        path = "/prometheus"
      }

      liveness_probe {
        http_get {
          path = "/-/healthy"
          port = 9090
        }
        initial_delay_seconds = 30
        period_seconds        = 10
        timeout_seconds       = 5
        failure_threshold     = 3
      }

      readiness_probe {
        http_get {
          path = "/-/ready"
          port = 9090
        }
        initial_delay_seconds = 10
        period_seconds        = 5
        timeout_seconds       = 3
        failure_threshold     = 3
      }
    }

    volume {
      name         = "prometheus-config"
      storage_type = "AzureFile"
      storage_name = azurerm_storage_share.prometheus_config[0].name
    }

    volume {
      name         = "prometheus-data"
      storage_type = "AzureFile"
      storage_name = azurerm_storage_share.prometheus_data[0].name
    }
  }

  ingress {
    allow_insecure_connections = false
    external_enabled           = false
    target_port               = 9090

    traffic_weight {
      percentage      = 100
      latest_revision = true
    }
  }

  tags = var.tags
}

resource "azurerm_container_app" "grafana" {
  count                        = var.enable_monitoring ? 1 : 0
  name                         = "${var.project_name}-grafana-${var.environment}"
  container_app_environment_id = azurerm_container_app_environment.main.id
  resource_group_name          = azurerm_resource_group.main.name
  revision_mode                = "Single"

  template {
    min_replicas = 1
    max_replicas = 1

    container {
      name   = "grafana"
      image  = "grafana/grafana:9.3.0"
      cpu    = var.grafana_cpu
      memory = "${var.grafana_memory}Gi"

      env {
        name  = "GF_SECURITY_ADMIN_USER"
        value = "admin"
      }

      env {
        name  = "GF_SECURITY_ADMIN_PASSWORD"
        value = var.grafana_admin_password
      }

      env {
        name  = "GF_INSTALL_PLUGINS"
        value = "grafana-piechart-panel,grafana-worldmap-panel,grafana-clock-panel"
      }

      env {
        name  = "GF_PATHS_PROVISIONING"
        value = "/etc/grafana/provisioning"
      }

      volume_mounts {
        name = "grafana-data"
        path = "/var/lib/grafana"
      }

      volume_mounts {
        name = "grafana-config"
        path = "/etc/grafana/provisioning"
      }

      liveness_probe {
        http_get {
          path = "/api/health"
          port = 3000
        }
        initial_delay_seconds = 30
        period_seconds        = 10
        timeout_seconds       = 5
        failure_threshold     = 3
      }

      readiness_probe {
        http_get {
          path = "/api/health"
          port = 3000
        }
        initial_delay_seconds = 10
        period_seconds        = 5
        timeout_seconds       = 3
        failure_threshold     = 3
      }
    }

    volume {
      name         = "grafana-data"
      storage_type = "AzureFile"
      storage_name = azurerm_storage_share.grafana_data[0].name
    }

    volume {
      name         = "grafana-config"
      storage_type = "AzureFile"
      storage_name = azurerm_storage_share.grafana_config[0].name
    }
  }

  ingress {
    allow_insecure_connections = false
    external_enabled           = true
    target_port               = 3000

    traffic_weight {
      percentage      = 100
      latest_revision = true
    }
  }

  tags = var.tags
}

# Azure Files Storage for Monitoring
resource "azurerm_storage_share" "prometheus_config" {
  count                = var.enable_monitoring ? 1 : 0
  name                 = "${var.project_name}-prometheus-config-${var.environment}"
  storage_account_name = azurerm_storage_account.main.name
  quota                = 1

  metadata = {
    environment = var.environment
    service     = "prometheus"
  }
}

resource "azurerm_storage_share" "prometheus_data" {
  count                = var.enable_monitoring ? 1 : 0
  name                 = "${var.project_name}-prometheus-data-${var.environment}"
  storage_account_name = azurerm_storage_account.main.name
  quota                = var.prometheus_storage_size

  metadata = {
    environment = var.environment
    service     = "prometheus"
  }
}

resource "azurerm_storage_share" "grafana_data" {
  count                = var.enable_monitoring ? 1 : 0
  name                 = "${var.project_name}-grafana-data-${var.environment}"
  storage_account_name = azurerm_storage_account.main.name
  quota                = var.grafana_storage_size

  metadata = {
    environment = var.environment
    service     = "grafana"
  }
}

resource "azurerm_storage_share" "grafana_config" {
  count                = var.enable_monitoring ? 1 : 0
  name                 = "${var.project_name}-grafana-config-${var.environment}"
  storage_account_name = azurerm_storage_account.main.name
  quota                = 1

  metadata = {
    environment = var.environment
    service     = "grafana"
  }
}

# Application Insights for Azure-native monitoring
resource "azurerm_application_insights" "latticedb" {
  count               = var.enable_monitoring ? 1 : 0
  name                = "${var.project_name}-insights-${var.environment}"
  location            = azurerm_resource_group.main.location
  resource_group_name = azurerm_resource_group.main.name
  application_type    = "web"
  retention_in_days   = var.insights_retention_days

  tags = var.tags
}

# Monitor Action Groups for Alerting
resource "azurerm_monitor_action_group" "latticedb" {
  count               = var.enable_monitoring ? 1 : 0
  name                = "${var.project_name}-alerts-${var.environment}"
  resource_group_name = azurerm_resource_group.main.name
  short_name          = "latticedb"

  email_receiver {
    name          = "admin"
    email_address = var.alert_email
  }

  dynamic "webhook_receiver" {
    for_each = var.slack_webhook_url != "" ? [1] : []
    content {
      name        = "slack"
      service_uri = var.slack_webhook_url
    }
  }

  tags = var.tags
}

# Metric Alerts
resource "azurerm_monitor_metric_alert" "high_cpu" {
  count               = var.enable_monitoring ? 1 : 0
  name                = "${var.project_name}-high-cpu-${var.environment}"
  resource_group_name = azurerm_resource_group.main.name
  scopes              = [azurerm_container_app.latticedb.id]
  description         = "High CPU usage alert"
  severity            = 2
  frequency           = "PT5M"
  window_size         = "PT15M"

  criteria {
    metric_namespace = "Microsoft.App/containerApps"
    metric_name      = "CpuPercentage"
    aggregation      = "Average"
    operator         = "GreaterThan"
    threshold        = 80
  }

  action {
    action_group_id = azurerm_monitor_action_group.latticedb[0].id
  }

  tags = var.tags
}

resource "azurerm_monitor_metric_alert" "high_memory" {
  count               = var.enable_monitoring ? 1 : 0
  name                = "${var.project_name}-high-memory-${var.environment}"
  resource_group_name = azurerm_resource_group.main.name
  scopes              = [azurerm_container_app.latticedb.id]
  description         = "High memory usage alert"
  severity            = 2
  frequency           = "PT5M"
  window_size         = "PT15M"

  criteria {
    metric_namespace = "Microsoft.App/containerApps"
    metric_name      = "MemoryPercentage"
    aggregation      = "Average"
    operator         = "GreaterThan"
    threshold        = 85
  }

  action {
    action_group_id = azurerm_monitor_action_group.latticedb[0].id
  }

  tags = var.tags
}