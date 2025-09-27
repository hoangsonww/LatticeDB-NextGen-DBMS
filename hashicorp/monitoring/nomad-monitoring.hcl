# Nomad job for monitoring stack deployment
job "monitoring" {
  datacenters = ["dc1"]
  type        = "service"
  priority    = 75

  group "prometheus" {
    count = 1

    network {
      port "prometheus_ui" {
        static = 9090
        to     = 9090
      }
    }

    volume "prometheus-data" {
      type            = "csi"
      source          = "prometheus-data"
      attachment_mode = "file-system"
      access_mode     = "single-node-writer"
      read_only       = false
    }

    service {
      name = "prometheus"
      port = "prometheus_ui"
      tags = [
        "monitoring",
        "prometheus",
        "metrics",
        "urlprefix-/prometheus",
        "traefik.enable=true",
        "traefik.http.routers.prometheus.rule=Host(`monitoring.service.consul`)"
      ]

      check {
        type     = "http"
        path     = "/prometheus/-/healthy"
        interval = "30s"
        timeout  = "5s"
        port     = "prometheus_ui"
      }

      connect {
        sidecar_service {
          proxy {
            upstreams {
              destination_name = "consul"
              local_bind_port  = 8500
            }

            upstreams {
              destination_name = "nomad"
              local_bind_port  = 4646
            }

            upstreams {
              destination_name = "vault"
              local_bind_port  = 8200
            }
          }
        }
      }
    }

    task "prometheus" {
      driver = "docker"

      volume_mount {
        volume      = "prometheus-data"
        destination = "/prometheus"
        read_only   = false
      }

      config {
        image = "prom/prometheus:v2.40.0"
        ports = ["prometheus_ui"]

        args = [
          "--config.file=/etc/prometheus/prometheus.yml",
          "--storage.tsdb.path=/prometheus",
          "--storage.tsdb.retention.time=30d",
          "--web.console.libraries=/etc/prometheus/console_libraries",
          "--web.console.templates=/etc/prometheus/consoles",
          "--web.enable-lifecycle",
          "--web.route-prefix=/prometheus",
          "--web.external-url=http://monitoring.service.consul/prometheus"
        ]

        mount {
          type     = "bind"
          target   = "/etc/prometheus/prometheus.yml"
          source   = "local/prometheus.yml"
          readonly = true
        }

        mount {
          type     = "bind"
          target   = "/etc/prometheus/alerts"
          source   = "local/alerts"
          readonly = true
        }
      }

      # Prometheus configuration template
      template {
        data = file("prometheus.yml")
        destination = "local/prometheus.yml"
        change_mode = "signal"
        change_signal = "SIGHUP"
      }

      # Alert rules
      template {
        data = file("alerts/latticedb.yml")
        destination = "local/alerts/latticedb.yml"
        change_mode = "signal"
        change_signal = "SIGHUP"
      }

      template {
        data = file("alerts/hashicorp-stack.yml")
        destination = "local/alerts/hashicorp-stack.yml"
        change_mode = "signal"
        change_signal = "SIGHUP"
      }

      resources {
        cpu    = 1000
        memory = 2048
      }

      vault {
        policies = ["monitoring-policy"]
      }
    }
  }

  group "grafana" {
    count = 1

    network {
      port "grafana_ui" {
        static = 3000
        to     = 3000
      }
    }

    volume "grafana-data" {
      type            = "csi"
      source          = "grafana-data"
      attachment_mode = "file-system"
      access_mode     = "single-node-writer"
      read_only       = false
    }

    service {
      name = "grafana"
      port = "grafana_ui"
      tags = [
        "monitoring",
        "grafana",
        "dashboards",
        "urlprefix-/grafana",
        "traefik.enable=true",
        "traefik.http.routers.grafana.rule=Host(`monitoring.service.consul`)"
      ]

      check {
        type     = "http"
        path     = "/api/health"
        interval = "30s"
        timeout  = "5s"
        port     = "grafana_ui"
      }

      connect {
        sidecar_service {
          proxy {
            upstreams {
              destination_name = "prometheus"
              local_bind_port  = 9090
            }
          }
        }
      }
    }

    task "grafana" {
      driver = "docker"

      volume_mount {
        volume      = "grafana-data"
        destination = "/var/lib/grafana"
        read_only   = false
      }

      config {
        image = "grafana/grafana:9.3.0"
        ports = ["grafana_ui"]

        mount {
          type     = "bind"
          target   = "/etc/grafana/grafana.ini"
          source   = "local/grafana.ini"
          readonly = true
        }

        mount {
          type     = "bind"
          target   = "/etc/grafana/provisioning"
          source   = "local/provisioning"
          readonly = true
        }
      }

      # Grafana configuration
      template {
        data = <<EOH
[server]
domain = monitoring.service.consul
root_url = http://monitoring.service.consul/grafana
serve_from_sub_path = true

[database]
type = sqlite3
path = /var/lib/grafana/grafana.db

[security]
admin_user = admin
admin_password = {{ with secret "secret/monitoring/grafana" }}{{ .Data.data.admin_password }}{{ end }}

[auth]
disable_login_form = false

[auth.anonymous]
enabled = false

[alerting]
enabled = true

[unified_alerting]
enabled = true

[log]
mode = console
level = info

[metrics]
enabled = true

[feature_toggles]
enable = ngalert
EOH
        destination = "local/grafana.ini"
        change_mode = "restart"
        perms       = "644"
      }

      # Datasource provisioning
      template {
        data = <<EOH
apiVersion: 1

datasources:
  - name: Prometheus
    type: prometheus
    url: http://127.0.0.1:9090/prometheus
    access: proxy
    isDefault: true
    editable: true
    jsonData:
      timeInterval: 15s
EOH
        destination = "local/provisioning/datasources/prometheus.yml"
        change_mode = "restart"
        perms       = "644"
      }

      # Dashboard provisioning
      template {
        data = <<EOH
apiVersion: 1

providers:
  - name: 'default'
    orgId: 1
    folder: ''
    type: file
    disableDeletion: false
    updateIntervalSeconds: 10
    allowUiUpdates: true
    options:
      path: /etc/grafana/provisioning/dashboards
EOH
        destination = "local/provisioning/dashboards/dashboards.yml"
        change_mode = "restart"
        perms       = "644"
      }

      # LatticeDB dashboard
      template {
        data = file("grafana-dashboard.json")
        destination = "local/provisioning/dashboards/latticedb.json"
        change_mode = "restart"
        perms       = "644"
      }

      env {
        GF_PATHS_PROVISIONING = "/etc/grafana/provisioning"
        GF_INSTALL_PLUGINS = "grafana-piechart-panel,grafana-worldmap-panel"
      }

      resources {
        cpu    = 500
        memory = 1024
      }

      vault {
        policies = ["monitoring-policy"]
      }
    }
  }

  group "alertmanager" {
    count = 1

    network {
      port "alertmanager_ui" {
        static = 9093
        to     = 9093
      }

      port "alertmanager_cluster" {
        static = 9094
        to     = 9094
      }
    }

    volume "alertmanager-data" {
      type            = "csi"
      source          = "alertmanager-data"
      attachment_mode = "file-system"
      access_mode     = "single-node-writer"
      read_only       = false
    }

    service {
      name = "alertmanager"
      port = "alertmanager_ui"
      tags = [
        "monitoring",
        "alertmanager",
        "alerts",
        "urlprefix-/alertmanager",
        "traefik.enable=true",
        "traefik.http.routers.alertmanager.rule=Host(`monitoring.service.consul`)"
      ]

      check {
        type     = "http"
        path     = "/alertmanager/-/healthy"
        interval = "30s"
        timeout  = "5s"
        port     = "alertmanager_ui"
      }

      connect {
        sidecar_service {}
      }
    }

    task "alertmanager" {
      driver = "docker"

      volume_mount {
        volume      = "alertmanager-data"
        destination = "/alertmanager"
        read_only   = false
      }

      config {
        image = "prom/alertmanager:v0.25.0"
        ports = ["alertmanager_ui", "alertmanager_cluster"]

        args = [
          "--config.file=/etc/alertmanager/alertmanager.yml",
          "--storage.path=/alertmanager",
          "--web.external-url=http://monitoring.service.consul/alertmanager",
          "--web.route-prefix=/alertmanager",
          "--cluster.listen-address=0.0.0.0:9094"
        ]

        mount {
          type     = "bind"
          target   = "/etc/alertmanager/alertmanager.yml"
          source   = "local/alertmanager.yml"
          readonly = true
        }
      }

      # Alertmanager configuration
      template {
        data = <<EOH
global:
  smtp_smarthost: '{{ with secret "secret/monitoring/smtp" }}{{ .Data.data.host }}:{{ .Data.data.port }}{{ end }}'
  smtp_from: '{{ with secret "secret/monitoring/smtp" }}{{ .Data.data.from }}{{ end }}'
  smtp_auth_username: '{{ with secret "secret/monitoring/smtp" }}{{ .Data.data.username }}{{ end }}'
  smtp_auth_password: '{{ with secret "secret/monitoring/smtp" }}{{ .Data.data.password }}{{ end }}'

route:
  group_by: ['alertname', 'cluster', 'service']
  group_wait: 10s
  group_interval: 10s
  repeat_interval: 1h
  receiver: 'default'
  routes:
  - match:
      severity: critical
    receiver: 'critical-alerts'
  - match:
      service: latticedb
    receiver: 'latticedb-alerts'

receivers:
- name: 'default'
  email_configs:
  - to: '{{ with secret "secret/monitoring/notifications" }}{{ .Data.data.default_email }}{{ end }}'
    subject: '[ALERT] {{`{{ .GroupLabels.alertname }}`}}'
    body: |
      {{`{{ range .Alerts }}`}}
      Alert: {{`{{ .Annotations.summary }}`}}
      Description: {{`{{ .Annotations.description }}`}}
      {{`{{ end }}`}}

- name: 'critical-alerts'
  email_configs:
  - to: '{{ with secret "secret/monitoring/notifications" }}{{ .Data.data.critical_email }}{{ end }}'
    subject: '[CRITICAL] {{`{{ .GroupLabels.alertname }}`}}'
    body: |
      CRITICAL ALERT TRIGGERED

      {{`{{ range .Alerts }}`}}
      Alert: {{`{{ .Annotations.summary }}`}}
      Description: {{`{{ .Annotations.description }}`}}
      Runbook: {{`{{ .Annotations.runbook_url }}`}}
      {{`{{ end }}`}}

- name: 'latticedb-alerts'
  email_configs:
  - to: '{{ with secret "secret/monitoring/notifications" }}{{ .Data.data.latticedb_email }}{{ end }}'
    subject: '[LatticeDB] {{`{{ .GroupLabels.alertname }}`}}'
    body: |
      LatticeDB Alert

      {{`{{ range .Alerts }}`}}
      Alert: {{`{{ .Annotations.summary }}`}}
      Description: {{`{{ .Annotations.description }}`}}
      Instance: {{`{{ .Labels.instance }}`}}
      {{`{{ end }}`}}

inhibit_rules:
  - source_match:
      severity: 'critical'
    target_match:
      severity: 'warning'
    equal: ['alertname', 'instance']
EOH
        destination = "local/alertmanager.yml"
        change_mode = "restart"
        perms       = "644"
      }

      resources {
        cpu    = 200
        memory = 512
      }

      vault {
        policies = ["monitoring-policy"]
      }
    }
  }

  group "exporters" {
    count = 1

    network {
      port "node_exporter" {
        static = 9100
        to     = 9100
      }

      port "cadvisor" {
        static = 8080
        to     = 8080
      }
    }

    service {
      name = "node-exporter"
      port = "node_exporter"
      tags = ["monitoring", "node-exporter", "metrics"]

      check {
        type     = "http"
        path     = "/metrics"
        interval = "30s"
        timeout  = "5s"
        port     = "node_exporter"
      }
    }

    service {
      name = "cadvisor"
      port = "cadvisor"
      tags = ["monitoring", "cadvisor", "container-metrics"]

      check {
        type     = "http"
        path     = "/healthz"
        interval = "30s"
        timeout  = "5s"
        port     = "cadvisor"
      }
    }

    task "node-exporter" {
      driver = "docker"

      config {
        image = "prom/node-exporter:v1.5.0"
        ports = ["node_exporter"]

        args = [
          "--path.procfs=/host/proc",
          "--path.sysfs=/host/sys",
          "--path.rootfs=/host",
          "--collector.filesystem.ignored-mount-points",
          "^/(sys|proc|dev|host|etc|rootfs/var/lib/docker/containers|rootfs/var/lib/docker/overlay2|rootfs/run/docker/netns|rootfs/var/lib/docker/aufs)($$|/)"
        ]

        volumes = [
          "/proc:/host/proc:ro",
          "/sys:/host/sys:ro",
          "/:/host:ro,rslave"
        ]

        privileged = true
        pid_mode   = "host"
        network_mode = "host"
      }

      resources {
        cpu    = 100
        memory = 128
      }
    }

    task "cadvisor" {
      driver = "docker"

      config {
        image = "gcr.io/cadvisor/cadvisor:v0.46.0"
        ports = ["cadvisor"]

        volumes = [
          "/:/rootfs:ro",
          "/var/run:/var/run:rw",
          "/sys:/sys:ro",
          "/var/lib/docker/:/var/lib/docker:ro",
          "/dev/disk/:/dev/disk:ro"
        ]

        privileged = true
      }

      resources {
        cpu    = 200
        memory = 256
      }
    }
  }
}