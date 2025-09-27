terraform {
  required_version = ">= 1.0"
  required_providers {
    google = {
      source  = "hashicorp/google"
      version = "~> 5.0"
    }
    google-beta = {
      source  = "hashicorp/google-beta"
      version = "~> 5.0"
    }
  }
  backend "gcs" {
    # Configure backend via terraform init -backend-config
    # bucket = "your-terraform-state-bucket"
    # prefix = "latticedb/terraform.tfstate"
  }
}

provider "google" {
  project = var.project_id
  region  = var.gcp_region
}

provider "google-beta" {
  project = var.project_id
  region  = var.gcp_region
}

# Data sources
data "google_project" "project" {}

# Local values
locals {
  cluster_name = "${var.project_name}-${var.environment}"
  common_labels = merge(var.labels, {
    project     = var.project_name
    environment = var.environment
    managed-by  = "terraform"
  })
}

# Enable required APIs
resource "google_project_service" "apis" {
  for_each = toset([
    "run.googleapis.com",
    "cloudbuild.googleapis.com",
    "artifactregistry.googleapis.com",
    "cloudresourcemanager.googleapis.com",
    "iam.googleapis.com",
    "monitoring.googleapis.com",
    "logging.googleapis.com",
    "storage.googleapis.com",
    "secretmanager.googleapis.com",
    "compute.googleapis.com",
    "vpcaccess.googleapis.com",
    var.enable_cloud_sql ? "sqladmin.googleapis.com" : ""
  ])

  service = each.value
  project = var.project_id

  disable_dependent_services = true
  disable_on_destroy         = false

  count = each.value != "" ? 1 : 0
}

# VPC Network
resource "google_compute_network" "main" {
  name                    = "${local.cluster_name}-vpc"
  auto_create_subnetworks = false
  mtu                     = 1460

  depends_on = [google_project_service.apis]
}

# Subnet for Cloud Run
resource "google_compute_subnetwork" "cloud_run" {
  name          = "${local.cluster_name}-cloud-run-subnet"
  ip_cidr_range = var.subnet_cidr
  region        = var.gcp_region
  network       = google_compute_network.main.id

  # Enable private Google access for Cloud Run
  private_ip_google_access = true

  # Secondary range for Cloud SQL (if enabled)
  dynamic "secondary_ip_range" {
    for_each = var.enable_cloud_sql ? [1] : []
    content {
      range_name    = "cloud-sql-range"
      ip_cidr_range = var.cloud_sql_cidr
    }
  }
}

# VPC Connector for Cloud Run
resource "google_vpc_access_connector" "main" {
  name           = "${local.cluster_name}-connector"
  ip_cidr_range  = var.vpc_connector_cidr
  network        = google_compute_network.main.name
  region         = var.gcp_region
  machine_type   = "e2-micro"
  min_instances  = 2
  max_instances  = 3

  depends_on = [google_project_service.apis]
}

# Firewall rules
resource "google_compute_firewall" "allow_cloud_run" {
  name    = "${local.cluster_name}-allow-cloud-run"
  network = google_compute_network.main.name

  allow {
    protocol = "tcp"
    ports    = ["8080", "5432"]
  }

  source_ranges = [var.subnet_cidr, var.vpc_connector_cidr]
  target_tags   = ["cloud-run"]
}

# Artifact Registry
resource "google_artifact_registry_repository" "main" {
  location      = var.gcp_region
  repository_id = "${var.project_name}-${var.environment}"
  description   = "Docker repository for ${var.project_name}"
  format        = "DOCKER"

  cleanup_policies {
    id     = "keep-latest"
    action = "KEEP"
    condition {
      tag_state  = "TAGGED"
      tag_prefixes = ["v"]
      older_than = "2592000s"  # 30 days
    }
  }

  cleanup_policies {
    id     = "delete-untagged"
    action = "DELETE"
    condition {
      tag_state  = "UNTAGGED"
      older_than = "604800s"  # 7 days
    }
  }

  labels = local.common_labels

  depends_on = [google_project_service.apis]
}

# Service Account for Cloud Run
resource "google_service_account" "cloud_run" {
  account_id   = "${local.cluster_name}-sa"
  display_name = "Cloud Run Service Account for ${local.cluster_name}"
  description  = "Service account for running LatticeDB on Cloud Run"
}

# IAM bindings for service account
resource "google_project_iam_member" "cloud_run_logging" {
  project = var.project_id
  role    = "roles/logging.logWriter"
  member  = "serviceAccount:${google_service_account.cloud_run.email}"
}

resource "google_project_iam_member" "cloud_run_monitoring" {
  project = var.project_id
  role    = "roles/monitoring.metricWriter"
  member  = "serviceAccount:${google_service_account.cloud_run.email}"
}

resource "google_project_iam_member" "cloud_run_trace" {
  project = var.project_id
  role    = "roles/cloudtrace.agent"
  member  = "serviceAccount:${google_service_account.cloud_run.email}"
}

# Secret Manager access (if enabled)
resource "google_project_iam_member" "cloud_run_secrets" {
  count   = var.enable_secret_manager ? 1 : 0
  project = var.project_id
  role    = "roles/secretmanager.secretAccessor"
  member  = "serviceAccount:${google_service_account.cloud_run.email}"
}

# Cloud Storage bucket for persistent data
resource "google_storage_bucket" "data" {
  count    = var.enable_cloud_storage ? 1 : 0
  name     = "${var.project_id}-${local.cluster_name}-data"
  location = var.gcp_region

  uniform_bucket_level_access = true
  force_destroy              = var.environment != "production"

  versioning {
    enabled = true
  }

  lifecycle_rule {
    condition {
      age = var.backup_retention_days
    }
    action {
      type = "Delete"
    }
  }

  encryption {
    default_kms_key_name = var.kms_key_name
  }

  labels = local.common_labels
}

# IAM binding for Cloud Storage
resource "google_storage_bucket_iam_member" "cloud_run_storage" {
  count  = var.enable_cloud_storage ? 1 : 0
  bucket = google_storage_bucket.data[0].name
  role   = "roles/storage.objectAdmin"
  member = "serviceAccount:${google_service_account.cloud_run.email}"
}

# Cloud SQL instance (optional)
resource "google_sql_database_instance" "main" {
  count            = var.enable_cloud_sql ? 1 : 0
  name             = "${local.cluster_name}-db"
  database_version = var.cloud_sql_version
  region           = var.gcp_region

  deletion_protection = var.environment == "production"

  settings {
    tier              = var.cloud_sql_tier
    availability_type = var.environment == "production" ? "REGIONAL" : "ZONAL"
    disk_type         = "PD_SSD"
    disk_size         = var.cloud_sql_disk_size
    disk_autoresize   = true

    backup_configuration {
      enabled                        = true
      start_time                     = "03:00"
      location                       = var.gcp_region
      point_in_time_recovery_enabled = var.environment == "production"
      transaction_log_retention_days = var.backup_retention_days
      backup_retention_settings {
        retained_backups = var.backup_retention_days
        retention_unit   = "COUNT"
      }
    }

    ip_configuration {
      ipv4_enabled    = false
      private_network = google_compute_network.main.id
      require_ssl     = true
    }

    database_flags {
      name  = "log_checkpoints"
      value = "on"
    }

    database_flags {
      name  = "log_connections"
      value = "on"
    }

    database_flags {
      name  = "log_disconnections"
      value = "on"
    }

    maintenance_window {
      day          = 7
      hour         = 3
      update_track = "stable"
    }
  }

  depends_on = [google_project_service.apis]
}

resource "google_sql_database" "latticedb" {
  count    = var.enable_cloud_sql ? 1 : 0
  name     = "latticedb"
  instance = google_sql_database_instance.main[0].name
}

resource "google_sql_user" "latticedb" {
  count    = var.enable_cloud_sql ? 1 : 0
  name     = "latticedb"
  instance = google_sql_database_instance.main[0].name
  password = var.db_password != "" ? var.db_password : random_password.db_password[0].result
}

resource "random_password" "db_password" {
  count   = var.enable_cloud_sql && var.db_password == "" ? 1 : 0
  length  = 32
  special = true
}

# Secret Manager secrets
resource "google_secret_manager_secret" "db_password" {
  count     = var.enable_cloud_sql && var.enable_secret_manager ? 1 : 0
  secret_id = "${local.cluster_name}-db-password"

  replication {
    auto {}
  }

  depends_on = [google_project_service.apis]
}

resource "google_secret_manager_secret_version" "db_password" {
  count       = var.enable_cloud_sql && var.enable_secret_manager ? 1 : 0
  secret      = google_secret_manager_secret.db_password[0].id
  secret_data = var.db_password != "" ? var.db_password : random_password.db_password[0].result
}

# Cloud Run service
resource "google_cloud_run_service" "latticedb" {
  name     = "${local.cluster_name}-service"
  location = var.gcp_region

  template {
    metadata {
      labels = local.common_labels
      annotations = {
        "autoscaling.knative.dev/minScale"                = tostring(var.min_instances)
        "autoscaling.knative.dev/maxScale"                = tostring(var.max_instances)
        "run.googleapis.com/vpc-access-connector"         = google_vpc_access_connector.main.name
        "run.googleapis.com/vpc-access-egress"            = "all-traffic"
        "run.googleapis.com/execution-environment"        = "gen2"
        "run.googleapis.com/cpu-throttling"               = "false"
      }
    }

    spec {
      service_account_name = google_service_account.cloud_run.email

      containers {
        image = "${var.gcp_region}-docker.pkg.dev/${var.project_id}/${google_artifact_registry_repository.main.repository_id}/${var.project_name}:${var.image_tag}"

        ports {
          container_port = 8080
        }

        resources {
          limits = {
            cpu    = var.cpu_limit
            memory = var.memory_limit
          }
        }

        env {
          name  = "LATTICEDB_ENV"
          value = var.environment
        }

        env {
          name  = "LATTICEDB_LOG_LEVEL"
          value = var.log_level
        }

        env {
          name  = "PORT"
          value = "8080"
        }

        # Cloud SQL connection (if enabled)
        dynamic "env" {
          for_each = var.enable_cloud_sql ? [1] : []
          content {
            name  = "DB_HOST"
            value = google_sql_database_instance.main[0].private_ip_address
          }
        }

        dynamic "env" {
          for_each = var.enable_cloud_sql ? [1] : []
          content {
            name  = "DB_NAME"
            value = google_sql_database.latticedb[0].name
          }
        }

        dynamic "env" {
          for_each = var.enable_cloud_sql ? [1] : []
          content {
            name  = "DB_USER"
            value = google_sql_user.latticedb[0].name
          }
        }

        # Database password from Secret Manager
        dynamic "env" {
          for_each = var.enable_cloud_sql && var.enable_secret_manager ? [1] : []
          content {
            name = "DB_PASSWORD"
            value_from {
              secret_key_ref {
                name = google_secret_manager_secret.db_password[0].secret_id
                key  = "latest"
              }
            }
          }
        }

        # Cloud Storage bucket (if enabled)
        dynamic "env" {
          for_each = var.enable_cloud_storage ? [1] : []
          content {
            name  = "STORAGE_BUCKET"
            value = google_storage_bucket.data[0].name
          }
        }

        startup_probe {
          http_get {
            path = "/health"
            port = 8080
          }
          initial_delay_seconds = 10
          timeout_seconds       = 3
          period_seconds        = 10
          failure_threshold     = 3
        }

        liveness_probe {
          http_get {
            path = "/health"
            port = 8080
          }
          initial_delay_seconds = 30
          timeout_seconds       = 3
          period_seconds        = 10
          failure_threshold     = 3
        }
      }
    }
  }

  traffic {
    percent         = 100
    latest_revision = true
  }

  autogenerate_revision_name = true

  depends_on = [google_project_service.apis]
}

# IAM policy for public access (if enabled)
resource "google_cloud_run_service_iam_member" "public" {
  count    = var.allow_unauthenticated ? 1 : 0
  location = google_cloud_run_service.latticedb.location
  project  = google_cloud_run_service.latticedb.project
  service  = google_cloud_run_service.latticedb.name
  role     = "roles/run.invoker"
  member   = "allUsers"
}

# Cloud Load Balancer (if custom domain is enabled)
resource "google_compute_global_address" "lb" {
  count = var.custom_domain != "" ? 1 : 0
  name  = "${local.cluster_name}-lb-ip"
}

# SSL Certificate
resource "google_compute_managed_ssl_certificate" "main" {
  count = var.custom_domain != "" ? 1 : 0
  name  = "${local.cluster_name}-ssl-cert"

  managed {
    domains = [var.custom_domain]
  }
}

# Backend service
resource "google_compute_backend_service" "main" {
  count                           = var.custom_domain != "" ? 1 : 0
  name                            = "${local.cluster_name}-backend"
  protocol                        = "HTTP"
  timeout_sec                     = 30
  connection_draining_timeout_sec = 30

  backend {
    group = google_compute_region_network_endpoint_group.cloud_run[0].id
  }

  health_checks = [google_compute_health_check.main[0].id]

  enable_cdn = var.enable_cdn
}

# Network endpoint group for Cloud Run
resource "google_compute_region_network_endpoint_group" "cloud_run" {
  count                 = var.custom_domain != "" ? 1 : 0
  name                  = "${local.cluster_name}-neg"
  network_endpoint_type = "SERVERLESS"
  region                = var.gcp_region

  cloud_run {
    service = google_cloud_run_service.latticedb.name
  }
}

# Health check
resource "google_compute_health_check" "main" {
  count               = var.custom_domain != "" ? 1 : 0
  name                = "${local.cluster_name}-health-check"
  check_interval_sec  = 10
  timeout_sec         = 5
  healthy_threshold   = 2
  unhealthy_threshold = 3

  http_health_check {
    port         = 8080
    request_path = "/health"
  }
}

# URL map
resource "google_compute_url_map" "main" {
  count           = var.custom_domain != "" ? 1 : 0
  name            = "${local.cluster_name}-url-map"
  default_service = google_compute_backend_service.main[0].id
}

# HTTPS proxy
resource "google_compute_target_https_proxy" "main" {
  count            = var.custom_domain != "" ? 1 : 0
  name             = "${local.cluster_name}-https-proxy"
  url_map          = google_compute_url_map.main[0].id
  ssl_certificates = [google_compute_managed_ssl_certificate.main[0].id]
}

# Global forwarding rule
resource "google_compute_global_forwarding_rule" "main" {
  count      = var.custom_domain != "" ? 1 : 0
  name       = "${local.cluster_name}-forwarding-rule"
  target     = google_compute_target_https_proxy.main[0].id
  port_range = "443"
  ip_address = google_compute_global_address.lb[0].id
}

# Monitoring Dashboard
resource "google_monitoring_dashboard" "main" {
  count          = var.enable_monitoring ? 1 : 0
  dashboard_json = templatefile("${path.module}/monitoring/dashboard.json.tpl", {
    project_id    = var.project_id
    service_name  = google_cloud_run_service.latticedb.name
    cluster_name  = local.cluster_name
  })

  depends_on = [google_project_service.apis]
}

# Alerting policies
resource "google_monitoring_alert_policy" "high_error_rate" {
  count        = var.enable_monitoring ? 1 : 0
  display_name = "${local.cluster_name} - High Error Rate"
  combiner     = "OR"

  conditions {
    display_name = "High error rate condition"

    condition_threshold {
      filter          = "resource.type=\"cloud_run_revision\" AND resource.label.service_name=\"${google_cloud_run_service.latticedb.name}\""
      duration        = "300s"
      comparison      = "COMPARISON_GREATER_THAN"
      threshold_value = 0.05

      aggregations {
        alignment_period   = "60s"
        per_series_aligner = "ALIGN_RATE"
        cross_series_reducer = "REDUCE_MEAN"
        group_by_fields = ["resource.label.service_name"]
      }
    }
  }

  notification_channels = var.notification_channels

  alert_strategy {
    auto_close = "86400s"  # 24 hours
  }
}

resource "google_monitoring_alert_policy" "high_latency" {
  count        = var.enable_monitoring ? 1 : 0
  display_name = "${local.cluster_name} - High Latency"
  combiner     = "OR"

  conditions {
    display_name = "High latency condition"

    condition_threshold {
      filter          = "resource.type=\"cloud_run_revision\" AND resource.label.service_name=\"${google_cloud_run_service.latticedb.name}\""
      duration        = "300s"
      comparison      = "COMPARISON_GREATER_THAN"
      threshold_value = 1000  # 1 second

      aggregations {
        alignment_period   = "60s"
        per_series_aligner = "ALIGN_DELTA"
        cross_series_reducer = "REDUCE_PERCENTILE_95"
        group_by_fields = ["resource.label.service_name"]
      }
    }
  }

  notification_channels = var.notification_channels

  alert_strategy {
    auto_close = "86400s"  # 24 hours
  }
}