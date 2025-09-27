job "latticedb" {
  datacenters = ["dc1"]
  type        = "service"
  priority    = 50

  # Specify the Nomad scheduler to use
  constraint {
    attribute = "${attr.kernel.name}"
    value     = "linux"
  }

  constraint {
    attribute = "${meta.node_class}"
    value     = "compute"
  }

  # Update strategy
  update {
    max_parallel      = 2
    min_healthy_time  = "30s"
    healthy_deadline  = "5m"
    progress_deadline = "10m"
    auto_revert       = true
    auto_promote      = true
    canary            = 1
    stagger           = "30s"
  }

  # Spread across multiple nodes for high availability
  spread {
    attribute = "${node.datacenter}"
    weight    = 100
  }

  spread {
    attribute = "${attr.platform.aws.instance-type}"
    weight    = 50
  }

  group "latticedb" {
    count = 3

    # Restart policy
    restart {
      attempts = 3
      interval = "5m"
      delay    = "25s"
      mode     = "fail"
    }

    # Reschedule policy
    reschedule {
      attempts       = 5
      interval       = "1h"
      delay          = "30s"
      delay_function = "exponential"
      max_delay      = "10m"
      unlimited      = false
    }

    # Anti-affinity to spread across nodes
    affinity {
      attribute = "${node.unique.id}"
      weight    = -100
    }

    # Persistent volume for data
    volume "latticedb-data" {
      type            = "csi"
      source          = "latticedb-data"
      attachment_mode = "file-system"
      access_mode     = "multi-node-multi-writer"
      read_only       = false

      mount_options {
        fs_type = "ext4"
      }
    }

    # Network configuration
    network {
      mode = "bridge"

      port "http" {
        static = 8080
        to     = 8080
      }

      port "sql" {
        static = 5432
        to     = 5432
      }

      # Health check port
      port "health" {
        static = 8081
        to     = 8081
      }
    }

    # Service mesh with Consul Connect
    service {
      name = "latticedb"
      port = "http"
      tags = [
        "database",
        "latticedb",
        "http",
        "${NOMAD_META_environment}",
        "urlprefix-/",
        "traefik.enable=true",
        "traefik.http.routers.latticedb.rule=Host(`${NOMAD_META_domain}`)",
        "traefik.http.routers.latticedb.tls=true"
      ]

      check {
        type     = "http"
        path     = "/health"
        interval = "10s"
        timeout  = "3s"
        port     = "http"
      }

      check {
        type     = "http"
        path     = "/ready"
        interval = "30s"
        timeout  = "5s"
        port     = "http"
      }

      # Consul Connect configuration
      connect {
        sidecar_service {
          proxy {
            upstreams {
              destination_name = "vault"
              local_bind_port  = 8200
            }

            upstreams {
              destination_name = "consul"
              local_bind_port  = 8500
            }

            config {
              protocol = "http"
            }
          }
        }
      }

      meta {
        version     = "${NOMAD_META_version}"
        environment = "${NOMAD_META_environment}"
        datacenter  = "${NOMAD_DC}"
      }
    }

    service {
      name = "latticedb-sql"
      port = "sql"
      tags = [
        "database",
        "latticedb",
        "sql",
        "postgres-compatible",
        "${NOMAD_META_environment}"
      ]

      check {
        type     = "tcp"
        interval = "10s"
        timeout  = "3s"
        port     = "sql"
      }

      connect {
        sidecar_service {}
      }

      meta {
        protocol = "postgresql"
        version  = "${NOMAD_META_version}"
      }
    }

    # Main application task
    task "latticedb" {
      driver = "docker"

      # Volume mount
      volume_mount {
        volume      = "latticedb-data"
        destination = "/var/lib/latticedb"
        read_only   = false
      }

      config {
        image = "${NOMAD_META_image}:${NOMAD_META_version}"
        ports = ["http", "sql", "health"]

        # Security configurations
        cap_add = [
          "IPC_LOCK",
          "SYS_RESOURCE"
        ]

        cap_drop = [
          "ALL"
        ]

        security_opt = [
          "no-new-privileges:true"
        ]

        # Logging configuration
        logging {
          type = "journald"
          config {
            tag = "latticedb"
            labels = "job,task,group,datacenter"
          }
        }

        # Mount points
        mount {
          type     = "bind"
          target   = "/etc/latticedb/ssl"
          source   = "secrets/ssl"
          readonly = true
        }

        mount {
          type     = "bind"
          target   = "/etc/latticedb/config"
          source   = "local/config"
          readonly = true
        }

        # Health check
        healthcheck {
          test     = ["CMD", "curl", "-f", "http://localhost:8080/health"]
          interval = "30s"
          timeout  = "5s"
          retries  = 3
          start_period = "60s"
        }
      }

      # Environment variables from Vault
      template {
        data = <<-EOH
          {{ with secret "secret/latticedb/${NOMAD_META_environment}/config" }}
          LATTICEDB_ENV="{{ .Data.data.environment }}"
          LATTICEDB_LOG_LEVEL="{{ .Data.data.log_level }}"
          LATTICEDB_DATA_DIR="/var/lib/latticedb"
          LATTICEDB_HTTP_PORT="8080"
          LATTICEDB_SQL_PORT="5432"
          LATTICEDB_HEALTH_PORT="8081"
          LATTICEDB_CLUSTER_NAME="{{ .Data.data.cluster_name }}"
          LATTICEDB_NODE_ID="{{ env "NOMAD_ALLOC_ID" }}"
          {{ end }}

          {{ with secret "secret/latticedb/${NOMAD_META_environment}/database" }}
          LATTICEDB_DB_PASSWORD="{{ .Data.data.password }}"
          LATTICEDB_ADMIN_PASSWORD="{{ .Data.data.admin_password }}"
          {{ end }}

          # Consul and Vault integration
          CONSUL_HTTP_ADDR="http://{{ env "NOMAD_IP_health" }}:8500"
          VAULT_ADDR="http://{{ env "NOMAD_IP_health" }}:8200"

          # TLS/SSL Configuration
          {{ with secret "secret/latticedb/${NOMAD_META_environment}/ssl" }}
          LATTICEDB_TLS_ENABLED="true"
          LATTICEDB_TLS_CERT_PATH="/etc/latticedb/ssl/cert.pem"
          LATTICEDB_TLS_KEY_PATH="/etc/latticedb/ssl/key.pem"
          LATTICEDB_TLS_CA_PATH="/etc/latticedb/ssl/ca.pem"
          {{ end }}
        EOH

        destination = "secrets/env"
        env         = true
        change_mode = "restart"
        perms       = "600"
      }

      # Configuration file template
      template {
        data = <<-EOH
          # LatticeDB Configuration
          [server]
          listen_address = "0.0.0.0"
          http_port = {{ env "LATTICEDB_HTTP_PORT" }}
          sql_port = {{ env "LATTICEDB_SQL_PORT" }}
          health_port = {{ env "LATTICEDB_HEALTH_PORT" }}

          [cluster]
          name = "{{ env "LATTICEDB_CLUSTER_NAME" }}"
          node_id = "{{ env "LATTICEDB_NODE_ID" }}"

          [storage]
          data_directory = "{{ env "LATTICEDB_DATA_DIR" }}"
          backup_enabled = true
          backup_interval = "1h"

          [logging]
          level = "{{ env "LATTICEDB_LOG_LEVEL" }}"
          format = "json"
          output = "stdout"

          [security]
          {{ if eq (env "LATTICEDB_TLS_ENABLED") "true" }}
          tls_enabled = true
          tls_cert_file = "{{ env "LATTICEDB_TLS_CERT_PATH" }}"
          tls_key_file = "{{ env "LATTICEDB_TLS_KEY_PATH" }}"
          tls_ca_file = "{{ env "LATTICEDB_TLS_CA_PATH" }}"
          {{ end }}

          [consul]
          enabled = true
          address = "{{ env "CONSUL_HTTP_ADDR" }}"
          service_name = "latticedb"

          [vault]
          enabled = true
          address = "{{ env "VAULT_ADDR" }}"
          path = "secret/latticedb"
        EOH

        destination = "local/config/latticedb.conf"
        change_mode = "restart"
        perms       = "644"
      }

      # SSL certificates from Vault
      template {
        data = <<-EOH
          {{ with secret "pki/issue/latticedb" "common_name=latticedb.service.consul" "ttl=24h" }}
          {{ .Data.certificate }}
          {{ end }}
        EOH

        destination = "secrets/ssl/cert.pem"
        change_mode = "restart"
        perms       = "600"
      }

      template {
        data = <<-EOH
          {{ with secret "pki/issue/latticedb" "common_name=latticedb.service.consul" "ttl=24h" }}
          {{ .Data.private_key }}
          {{ end }}
        EOH

        destination = "secrets/ssl/key.pem"
        change_mode = "restart"
        perms       = "600"
      }

      template {
        data = <<-EOH
          {{ with secret "pki/issue/latticedb" "common_name=latticedb.service.consul" "ttl=24h" }}
          {{ .Data.issuing_ca }}
          {{ end }}
        EOH

        destination = "secrets/ssl/ca.pem"
        change_mode = "restart"
        perms       = "644"
      }

      # Resource requirements
      resources {
        cpu    = 1000 # MHz
        memory = 2048 # MB

        device "nvidia/gpu" {
          count = 0
        }
      }

      # Vault integration
      vault {
        policies = ["latticedb-policy"]
      }

      # Lifecycle hooks
      lifecycle {
        hook    = "prestart"
        sidecar = false
      }

      # Shutdown grace period
      kill_timeout = "30s"
      kill_signal  = "SIGTERM"

      # Artifact for additional files
      artifact {
        source      = "https://releases.latticedb.com/scripts/init.sh"
        destination = "local/scripts/"
        options {
          checksum = "sha256:abc123..."
        }
      }

      # Log collection
      logs {
        max_files     = 5
        max_file_size = 10
      }
    }

    # Sidecar for log forwarding
    task "log-shipper" {
      driver = "docker"

      lifecycle {
        hook    = "poststart"
        sidecar = true
      }

      config {
        image = "fluent/fluent-bit:latest"

        mount {
          type     = "bind"
          target   = "/fluent-bit/etc/fluent-bit.conf"
          source   = "local/fluent-bit.conf"
          readonly = true
        }

        mount {
          type     = "bind"
          target   = "/var/log"
          source   = "/opt/nomad/alloc/${NOMAD_ALLOC_ID}/alloc/logs"
          readonly = true
        }
      }

      template {
        data = <<-EOH
          [SERVICE]
              Flush         1
              Log_Level     info
              Daemon        off
              Parsers_File  parsers.conf

          [INPUT]
              Name              tail
              Path              /var/log/latticedb.*.log
              Parser            json
              Tag               latticedb.*
              Refresh_Interval  5

          [OUTPUT]
              Name  http
              Match *
              Host  {{ env "NOMAD_IP_health" }}
              Port  8428
              URI   /api/v1/write
              Format json
        EOH

        destination = "local/fluent-bit.conf"
        change_mode = "restart"
      }

      resources {
        cpu    = 100
        memory = 128
      }
    }
  }
}