# Production environment variables for HashiCorp Stack
environment = "prod"
datacenter = "dc1"

# Consul Configuration
consul_datacenter = "dc1"
consul_bootstrap_expect = 5
consul_encrypt_key = "prod-consul-encrypt-key"
consul_log_level = "WARN"
consul_ui_enabled = true
consul_performance_raft_multiplier = 1

# Vault Configuration
vault_cluster_name = "latticedb-prod-vault"
vault_node_count = 5
vault_storage_backend = "consul"
vault_ui_enabled = true
vault_seal_type = "auto"
vault_auto_unseal_provider = "aws"

# Nomad Configuration
nomad_datacenter = "dc1"
nomad_bootstrap_expect = 5
nomad_log_level = "WARN"
nomad_node_class = "prod"
nomad_client_count = 10
nomad_server_count = 5

# LatticeDB Application Configuration
app_name = "latticedb-prod"
app_image = "latticedb:latest"
app_port = 8080
app_count = 5
app_cpu = 2000
app_memory = 4096

# Network Configuration
network_cidr = "10.1.0.0/16"
enable_connect = true
enable_tls = true
enable_gossip_encryption = true

# Storage Configuration
storage_type = "csi"
storage_size = "100Gi"
storage_class = "fast-ssd"

# Monitoring Configuration
enable_prometheus = true
prometheus_retention = "30d"
prometheus_storage_size = "50Gi"
enable_grafana = true
enable_alertmanager = true
alertmanager_pagerduty_key = "prod-pagerduty-key"
alertmanager_slack_webhook = "prod-webhook-url"

# Security Configuration
enable_acl = true
enable_encryption = true
enable_sentinel = true
enable_audit_logging = true

# High Availability Configuration
enable_cross_region_replication = true
enable_disaster_recovery = true

# Backup Configuration
enable_backup = true
backup_schedule = "0 2 * * *"
backup_retention_days = 30
enable_point_in_time_recovery = true

# Performance Configuration
enable_raft_snapshot_threshold = 8192
enable_raft_trailing_logs = 10240

# Tags
tags = {
  Environment = "prod"
  Project     = "latticedb"
  Owner       = "ops-team"
  Stack       = "hashicorp"
  Compliance  = "required"
  Backup      = "critical"
}