# Development environment variables for HashiCorp Stack
environment = "dev"
datacenter = "dc1"

# Consul Configuration
consul_datacenter = "dc1"
consul_bootstrap_expect = 1
consul_encrypt_key = "dev-consul-encrypt-key"
consul_log_level = "INFO"

# Vault Configuration
vault_cluster_name = "latticedb-dev-vault"
vault_node_count = 1
vault_storage_backend = "consul"
vault_ui_enabled = true

# Nomad Configuration
nomad_datacenter = "dc1"
nomad_bootstrap_expect = 1
nomad_log_level = "INFO"
nomad_node_class = "dev"

# LatticeDB Application Configuration
app_name = "latticedb-dev"
app_image = "latticedb:dev"
app_port = 8080
app_count = 1
app_cpu = 500
app_memory = 512

# Network Configuration
network_cidr = "10.0.0.0/16"
enable_connect = true
enable_tls = false

# Storage Configuration
storage_type = "host"
storage_path = "/opt/latticedb/data"

# Monitoring Configuration
enable_prometheus = true
prometheus_retention = "7d"
enable_grafana = true
enable_alertmanager = false

# Security Configuration
enable_acl = false
enable_encryption = false

# Tags
tags = {
  Environment = "dev"
  Project     = "latticedb"
  Owner       = "dev-team"
  Stack       = "hashicorp"
}