# Monitoring Infrastructure - Prometheus & Grafana for GCP

# Cloud Run Services for Monitoring
resource "google_cloud_run_service" "prometheus" {
  count    = var.enable_monitoring ? 1 : 0
  name     = "${var.project_name}-prometheus-${var.environment}"
  location = var.gcp_region

  template {
    spec {
      containers {
        image = "prom/prometheus:v2.40.0"

        args = [
          "--config.file=/etc/prometheus/prometheus.yml",
          "--storage.tsdb.path=/prometheus",
          "--storage.tsdb.retention.time=${var.prometheus_retention}",
          "--web.console.libraries=/etc/prometheus/console_libraries",
          "--web.console.templates=/etc/prometheus/consoles",
          "--web.enable-lifecycle"
        ]

        ports {
          container_port = 9090
        }

        env {
          name  = "LATTICEDB_ENVIRONMENT"
          value = var.environment
        }

        resources {
          limits = {
            cpu    = "${var.prometheus_cpu}m"
            memory = "${var.prometheus_memory}Mi"
          }
          requests = {
            cpu    = "${var.prometheus_cpu / 2}m"
            memory = "${var.prometheus_memory / 2}Mi"
          }
        }

        volume_mounts {
          mount_path = "/etc/prometheus"
          name       = "prometheus-config"
        }

        volume_mounts {
          mount_path = "/prometheus"
          name       = "prometheus-data"
        }

        startup_probe {
          http_get {
            path = "/-/healthy"
            port = 9090
          }
          initial_delay_seconds = 30
          period_seconds        = 10
          timeout_seconds       = 5
          failure_threshold     = 3
        }

        liveness_probe {
          http_get {
            path = "/-/healthy"
            port = 9090
          }
          period_seconds    = 10
          timeout_seconds   = 5
          failure_threshold = 3
        }
      }

      volumes {
        name = "prometheus-config"
        secret {
          secret_name = google_secret_manager_secret.prometheus_config[0].secret_id
        }
      }

      volumes {
        name = "prometheus-data"
        cloud_sql_instance {
          instances = []
        }
      }

      container_concurrency = 1000
      timeout_seconds      = 300
    }

    metadata {
      annotations = {
        "autoscaling.knative.dev/minScale" = "1"
        "autoscaling.knative.dev/maxScale" = "1"
        "run.googleapis.com/execution-environment" = "gen2"
      }
      labels = var.labels
    }
  }

  traffic {
    percent         = 100
    latest_revision = true
  }

  autogenerate_revision_name = true
}

resource "google_cloud_run_service" "grafana" {
  count    = var.enable_monitoring ? 1 : 0
  name     = "${var.project_name}-grafana-${var.environment}"
  location = var.gcp_region

  template {
    spec {
      containers {
        image = "grafana/grafana:9.3.0"

        ports {
          container_port = 3000
        }

        env {
          name  = "GF_SECURITY_ADMIN_USER"
          value = "admin"
        }

        env {
          name = "GF_SECURITY_ADMIN_PASSWORD"
          value_from {
            secret_key_ref {
              name = google_secret_manager_secret.grafana_password[0].secret_id
              key  = "latest"
            }
          }
        }

        env {
          name  = "GF_INSTALL_PLUGINS"
          value = "grafana-piechart-panel,grafana-worldmap-panel,grafana-clock-panel"
        }

        env {
          name  = "GF_PATHS_PROVISIONING"
          value = "/etc/grafana/provisioning"
        }

        resources {
          limits = {
            cpu    = "${var.grafana_cpu}m"
            memory = "${var.grafana_memory}Mi"
          }
          requests = {
            cpu    = "${var.grafana_cpu / 2}m"
            memory = "${var.grafana_memory / 2}Mi"
          }
        }

        volume_mounts {
          mount_path = "/var/lib/grafana"
          name       = "grafana-data"
        }

        volume_mounts {
          mount_path = "/etc/grafana/provisioning"
          name       = "grafana-config"
        }

        startup_probe {
          http_get {
            path = "/api/health"
            port = 3000
          }
          initial_delay_seconds = 30
          period_seconds        = 10
          timeout_seconds       = 5
          failure_threshold     = 3
        }

        liveness_probe {
          http_get {
            path = "/api/health"
            port = 3000
          }
          period_seconds    = 10
          timeout_seconds   = 5
          failure_threshold = 3
        }
      }

      volumes {
        name = "grafana-data"
        empty_dir {
          size_limit = "${var.grafana_storage_size}Gi"
        }
      }

      volumes {
        name = "grafana-config"
        secret {
          secret_name = google_secret_manager_secret.grafana_config[0].secret_id
        }
      }

      container_concurrency = 1000
      timeout_seconds      = 300
    }

    metadata {
      annotations = {
        "autoscaling.knative.dev/minScale" = "1"
        "autoscaling.knative.dev/maxScale" = "3"
        "run.googleapis.com/execution-environment" = "gen2"
      }
      labels = var.labels
    }
  }

  traffic {
    percent         = 100
    latest_revision = true
  }

  autogenerate_revision_name = true
}

# Secret Manager for Configuration
resource "google_secret_manager_secret" "prometheus_config" {
  count     = var.enable_monitoring ? 1 : 0
  secret_id = "${var.project_name}-prometheus-config-${var.environment}"

  replication {
    automatic = true
  }

  labels = var.labels
}

resource "google_secret_manager_secret_version" "prometheus_config" {
  count       = var.enable_monitoring ? 1 : 0
  secret      = google_secret_manager_secret.prometheus_config[0].id
  secret_data = templatefile("${path.module}/config/prometheus.yml", {
    environment = var.environment
    project_id  = var.project_id
  })
}

resource "google_secret_manager_secret" "grafana_password" {
  count     = var.enable_monitoring ? 1 : 0
  secret_id = "${var.project_name}-grafana-password-${var.environment}"

  replication {
    automatic = true
  }

  labels = var.labels
}

resource "google_secret_manager_secret_version" "grafana_password" {
  count       = var.enable_monitoring ? 1 : 0
  secret      = google_secret_manager_secret.grafana_password[0].id
  secret_data = var.grafana_admin_password
}

resource "google_secret_manager_secret" "grafana_config" {
  count     = var.enable_monitoring ? 1 : 0
  secret_id = "${var.project_name}-grafana-config-${var.environment}"

  replication {
    automatic = true
  }

  labels = var.labels
}

# Cloud Monitoring Alerting Policies
resource "google_monitoring_alert_policy" "high_cpu" {
  count        = var.enable_monitoring ? 1 : 0
  display_name = "${var.project_name} High CPU Usage - ${var.environment}"
  combiner     = "OR"
  enabled      = true

  conditions {
    display_name = "Cloud Run CPU utilization"

    condition_threshold {
      filter          = "resource.type=\"cloud_run_revision\" AND resource.labels.service_name=\"${var.project_name}-${var.environment}\""
      duration        = "300s"
      comparison      = "COMPARISON_GT"
      threshold_value = 0.8

      aggregations {
        alignment_period   = "60s"
        per_series_aligner = "ALIGN_MEAN"
      }
    }
  }

  notification_channels = var.notification_channels

  alert_strategy {
    auto_close = "86400s" # 24 hours
  }
}

resource "google_monitoring_alert_policy" "high_memory" {
  count        = var.enable_monitoring ? 1 : 0
  display_name = "${var.project_name} High Memory Usage - ${var.environment}"
  combiner     = "OR"
  enabled      = true

  conditions {
    display_name = "Cloud Run Memory utilization"

    condition_threshold {
      filter          = "resource.type=\"cloud_run_revision\" AND resource.labels.service_name=\"${var.project_name}-${var.environment}\""
      duration        = "300s"
      comparison      = "COMPARISON_GT"
      threshold_value = 0.85

      aggregations {
        alignment_period   = "60s"
        per_series_aligner = "ALIGN_MEAN"
      }
    }
  }

  notification_channels = var.notification_channels

  alert_strategy {
    auto_close = "86400s" # 24 hours
  }
}

resource "google_monitoring_alert_policy" "high_error_rate" {
  count        = var.enable_monitoring ? 1 : 0
  display_name = "${var.project_name} High Error Rate - ${var.environment}"
  combiner     = "OR"
  enabled      = true

  conditions {
    display_name = "Cloud Run Request latency"

    condition_threshold {
      filter          = "resource.type=\"cloud_run_revision\" AND resource.labels.service_name=\"${var.project_name}-${var.environment}\""
      duration        = "300s"
      comparison      = "COMPARISON_GT"
      threshold_value = 0.05

      aggregations {
        alignment_period   = "60s"
        per_series_aligner = "ALIGN_RATE"
      }
    }
  }

  notification_channels = var.notification_channels

  alert_strategy {
    auto_close = "86400s" # 24 hours
  }
}

# Uptime Check for LatticeDB Service
resource "google_monitoring_uptime_check_config" "latticedb" {
  count        = var.enable_monitoring ? 1 : 0
  display_name = "${var.project_name} Uptime Check - ${var.environment}"
  timeout      = "10s"
  period       = "60s"

  http_check {
    path         = "/health"
    port         = 443
    use_ssl      = true
    validate_ssl = true
  }

  monitored_resource {
    type = "cloud_run_revision"
    labels = {
      project_id   = var.project_id
      service_name = "${var.project_name}-${var.environment}"
      location     = var.gcp_region
    }
  }

  content_matchers {
    content = "OK"
    matcher = "CONTAINS_STRING"
  }
}

# IAM for Monitoring Services
resource "google_cloud_run_service_iam_binding" "prometheus_invoker" {
  count    = var.enable_monitoring ? 1 : 0
  location = google_cloud_run_service.prometheus[0].location
  project  = var.project_id
  service  = google_cloud_run_service.prometheus[0].name
  role     = "roles/run.invoker"
  members = [
    "serviceAccount:${google_service_account.latticedb.email}",
    "allUsers"
  ]
}

resource "google_cloud_run_service_iam_binding" "grafana_invoker" {
  count    = var.enable_monitoring ? 1 : 0
  location = google_cloud_run_service.grafana[0].location
  project  = var.project_id
  service  = google_cloud_run_service.grafana[0].name
  role     = "roles/run.invoker"
  members = [
    "serviceAccount:${google_service_account.latticedb.email}",
    "allUsers"
  ]
}