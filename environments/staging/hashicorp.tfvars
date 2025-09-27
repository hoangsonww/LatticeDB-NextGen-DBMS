# Staging environment variables for HashiCorp Stack
environment = "staging"
datacenter = "dc1"

# Consul Configuration
consul_datacenter = "dc1"
consul_bootstrap_expect = 3
consul_encrypt_key = "staging-consul-encrypt-key"
consul_log_level = "INFO"
consul_ui_enabled = true

# Vault Configuration
vault_cluster_name = "latticedb-staging-vault"
vault_node_count = 3
vault_storage_backend = "consul"
vault_ui_enabled = true
vault_seal_type = "shamir"

# Nomad Configuration
nomad_datacenter = "dc1"
nomad_bootstrap_expect = 3
nomad_log_level = "INFO"
nomad_node_class = "staging"
nomad_client_count = 3

# LatticeDB Application Configuration
app_name = "latticedb-staging"
app_image = "latticedb:staging"
app_port = 8080
app_count = 2
app_cpu = 1000
app_memory = 1024

# Network Configuration
network_cidr = "10.2.0.0/16"
enable_connect = true
enable_tls = true

# Storage Configuration
storage_type = "csi"
storage_size = "10Gi"

# Monitoring Configuration
enable_prometheus = true
prometheus_retention = "14d"
enable_grafana = true
enable_alertmanager = true
alertmanager_slack_webhook = "staging-webhook-url"

# Security Configuration
enable_acl = true
enable_encryption = true

# Backup Configuration
enable_backup = true
backup_schedule = "0 3 * * *"

# Tags
tags = {
  Environment = "staging"
  Project     = "latticedb"
  Owner       = "qa-team"
  Stack       = "hashicorp"
}