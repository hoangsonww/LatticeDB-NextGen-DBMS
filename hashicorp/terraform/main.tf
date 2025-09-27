terraform {
  required_version = ">= 1.0"
  required_providers {
    consul = {
      source  = "hashicorp/consul"
      version = "~> 2.18"
    }
    vault = {
      source  = "hashicorp/vault"
      version = "~> 3.20"
    }
    nomad = {
      source  = "hashicorp/nomad"
      version = "~> 2.0"
    }
    tls = {
      source  = "hashicorp/tls"
      version = "~> 4.0"
    }
    random = {
      source  = "hashicorp/random"
      version = "~> 3.4"
    }
  }
  backend "consul" {
    # Configure backend via terraform init -backend-config
    # address = "127.0.0.1:8500"
    # scheme  = "http"
    # path    = "terraform/latticedb"
  }
}

# Configure providers
provider "consul" {
  address    = var.consul_address
  datacenter = var.consul_datacenter
  token      = var.consul_token
}

provider "vault" {
  address   = var.vault_address
  token     = var.vault_token
  namespace = var.vault_namespace
}

provider "nomad" {
  address   = var.nomad_address
  region    = var.nomad_region
  token     = var.nomad_token
}

# Data sources
data "consul_datacenter" "current" {}

# Local values
locals {
  cluster_name = "${var.project_name}-${var.environment}"
  common_tags = merge(var.tags, {
    project     = var.project_name
    environment = var.environment
    managed_by  = "terraform"
    cluster     = local.cluster_name
  })
}

# Random passwords for initial setup
resource "random_password" "db_password" {
  length  = 32
  special = true
}

resource "random_password" "admin_password" {
  length  = 32
  special = true
}

resource "random_password" "encryption_key" {
  length  = 32
  special = true
}

# Vault secret engines
resource "vault_mount" "kv" {
  path        = "secret"
  type        = "kv"
  options     = { version = "2" }
  description = "KV Version 2 secret engine for LatticeDB"
}

resource "vault_mount" "pki_root" {
  path                      = "pki"
  type                      = "pki"
  description               = "Root PKI engine for LatticeDB"
  default_lease_ttl_seconds = 3600
  max_lease_ttl_seconds     = 315360000 # 10 years
}

resource "vault_mount" "pki_intermediate" {
  path                      = "pki_int"
  type                      = "pki"
  description               = "Intermediate PKI engine for LatticeDB"
  default_lease_ttl_seconds = 3600
  max_lease_ttl_seconds     = 31536000 # 1 year
}

resource "vault_mount" "database" {
  path        = "database"
  type        = "database"
  description = "Database secrets engine for LatticeDB"
}

resource "vault_mount" "transit" {
  path        = "transit"
  type        = "transit"
  description = "Transit secrets engine for LatticeDB encryption"
}

resource "vault_mount" "consul_secrets" {
  path        = "consul"
  type        = "consul"
  description = "Consul secrets engine"
}

resource "vault_mount" "nomad_secrets" {
  path        = "nomad"
  type        = "nomad"
  description = "Nomad secrets engine"
}

# PKI configuration
resource "vault_pki_secret_backend_root_cert" "root" {
  backend     = vault_mount.pki_root.path
  type        = "internal"
  common_name = "LatticeDB Root CA"
  ttl         = "315360000"
  format      = "pem"
  key_type    = "rsa"
  key_bits    = 4096
  issuer_name = "root-2024"

  depends_on = [vault_mount.pki_root]
}

resource "vault_pki_secret_backend_config_urls" "root_config" {
  backend                 = vault_mount.pki_root.path
  issuing_certificates    = ["${var.vault_address}/v1/pki/ca"]
  crl_distribution_points = ["${var.vault_address}/v1/pki/crl"]

  depends_on = [vault_pki_secret_backend_root_cert.root]
}

resource "vault_pki_secret_backend_intermediate_cert_request" "intermediate" {
  backend     = vault_mount.pki_intermediate.path
  type        = "internal"
  common_name = "LatticeDB Intermediate Authority"

  depends_on = [vault_mount.pki_intermediate]
}

resource "vault_pki_secret_backend_root_sign_intermediate" "intermediate" {
  backend     = vault_mount.pki_root.path
  csr         = vault_pki_secret_backend_intermediate_cert_request.intermediate.csr
  common_name = "LatticeDB Intermediate Authority"
  ttl         = "157680000" # 5 years
  format      = "pem_bundle"
}

resource "vault_pki_secret_backend_intermediate_set_signed" "intermediate" {
  backend     = vault_mount.pki_intermediate.path
  certificate = vault_pki_secret_backend_root_sign_intermediate.intermediate.certificate

  depends_on = [
    vault_pki_secret_backend_intermediate_cert_request.intermediate,
    vault_pki_secret_backend_root_sign_intermediate.intermediate
  ]
}

resource "vault_pki_secret_backend_config_urls" "intermediate_config" {
  backend                 = vault_mount.pki_intermediate.path
  issuing_certificates    = ["${var.vault_address}/v1/pki_int/ca"]
  crl_distribution_points = ["${var.vault_address}/v1/pki_int/crl"]

  depends_on = [vault_pki_secret_backend_intermediate_set_signed.intermediate]
}

resource "vault_pki_secret_backend_role" "latticedb" {
  backend         = vault_mount.pki_intermediate.path
  name            = "latticedb"
  allowed_domains = ["latticedb.service.consul", "localhost", "*.latticedb.local"]
  allow_subdomains = true
  max_ttl         = "720h"
  generate_lease  = true
  key_type        = "rsa"
  key_bits        = 2048

  depends_on = [vault_pki_secret_backend_intermediate_set_signed.intermediate]
}

# Transit encryption key
resource "vault_transit_secret_backend_key" "latticedb" {
  backend                = vault_mount.transit.path
  name                   = "latticedb"
  deletion_allowed       = false
  derived                = false
  exportable             = false
  allow_plaintext_backup = false
  type                   = "aes256-gcm96"
  auto_rotate_period     = 2160 # 90 days in hours
}

# Database secrets configuration
resource "vault_database_secrets_mount" "db" {
  path      = vault_mount.database.path

  postgresql {
    name           = "latticedb-postgres"
    username       = var.db_username
    password       = var.db_password != "" ? var.db_password : random_password.db_password.result
    connection_url = var.db_connection_url
    verify_connection = false
    allowed_roles  = ["latticedb-role"]
  }
}

resource "vault_database_secret_backend_role" "latticedb" {
  backend             = vault_mount.database.path
  name                = "latticedb-role"
  db_name             = vault_database_secrets_mount.db.postgresql[0].name
  default_ttl         = 3600
  max_ttl             = 86400
  creation_statements = [
    "CREATE ROLE \"{{name}}\" WITH LOGIN PASSWORD '{{password}}' VALID UNTIL '{{expiration}}';",
    "GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO \"{{name}}\";",
    "GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO \"{{name}}\";",
    "GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA public TO \"{{name}}\";"
  ]

  depends_on = [vault_database_secrets_mount.db]
}

# Vault policies
resource "vault_policy" "latticedb" {
  name   = "latticedb-policy"
  policy = file("${path.module}/../vault/policies.hcl")
}

resource "vault_policy" "latticedb_admin" {
  name = "latticedb-admin-policy"
  policy = <<EOT
# Full access to LatticeDB secrets
path "secret/data/latticedb/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

path "secret/metadata/latticedb/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# PKI access
path "pki*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# Database access
path "database/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# Transit access
path "transit/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# System access
path "sys/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}
EOT
}

# Auth methods
resource "vault_auth_backend" "approle" {
  type = "approle"
  path = "approle"
}

resource "vault_approle_auth_backend_role" "latticedb" {
  backend        = vault_auth_backend.approle.path
  role_name      = "latticedb"
  token_policies = [vault_policy.latticedb.name]
  token_ttl      = 3600
  token_max_ttl  = 14400
  bind_secret_id = true
}

resource "vault_approle_auth_backend_role_secret_id" "latticedb" {
  backend   = vault_auth_backend.approle.path
  role_name = vault_approle_auth_backend_role.latticedb.role_name
}

# Store secrets in Vault
resource "vault_kv_secret_v2" "database" {
  mount               = vault_mount.kv.path
  name                = "latticedb/${var.environment}/database"
  cas                 = 1
  delete_all_versions = true
  data_json = jsonencode({
    password       = var.db_password != "" ? var.db_password : random_password.db_password.result
    admin_password = var.admin_password != "" ? var.admin_password : random_password.admin_password.result
    encryption_key = random_password.encryption_key.result
  })
}

resource "vault_kv_secret_v2" "config" {
  mount               = vault_mount.kv.path
  name                = "latticedb/${var.environment}/config"
  cas                 = 1
  delete_all_versions = true
  data_json = jsonencode({
    environment      = var.environment
    log_level       = var.log_level
    cluster_name    = local.cluster_name
    http_port       = var.http_port
    sql_port        = var.sql_port
    health_port     = var.health_port
    max_connections = var.max_connections
    backup_enabled  = var.enable_backup
    backup_interval = var.backup_interval
  })
}

resource "vault_kv_secret_v2" "ssl" {
  mount               = vault_mount.kv.path
  name                = "latticedb/${var.environment}/ssl"
  cas                 = 1
  delete_all_versions = true
  data_json = jsonencode({
    tls_enabled     = var.enable_tls
    tls_min_version = "1.2"
    cipher_suites   = "ECDHE-RSA-AES256-GCM-SHA384,ECDHE-RSA-AES128-GCM-SHA256"
  })
}

# Consul configuration
resource "consul_key_prefix" "latticedb" {
  path_prefix = "latticedb/${var.environment}/"

  subkeys = {
    "cluster/name"     = local.cluster_name
    "cluster/region"   = var.consul_datacenter
    "config/log_level" = var.log_level
    "config/replicas"  = var.instance_count
  }
}

resource "consul_acl_policy" "latticedb" {
  count = var.enable_consul_acl ? 1 : 0

  name        = "latticedb-policy"
  description = "Policy for LatticeDB service"
  rules       = file("${path.module}/../consul/acl-policies.hcl")
  datacenters = [var.consul_datacenter]
}

resource "consul_acl_token" "latticedb" {
  count = var.enable_consul_acl ? 1 : 0

  description = "Token for LatticeDB service"
  policies    = [consul_acl_policy.latticedb[0].name]
  local       = false

  service_identities {
    service_name = "latticedb"
    datacenters  = [var.consul_datacenter]
  }

  service_identities {
    service_name = "latticedb-sql"
    datacenters  = [var.consul_datacenter]
  }

  service_identities {
    service_name = "latticedb-admin"
    datacenters  = [var.consul_datacenter]
  }
}

# Service mesh configuration
resource "consul_config_entry" "proxy_defaults" {
  kind = "proxy-defaults"
  name = "global"

  config_json = jsonencode({
    protocol = "http"
    config = {
      max_connections        = var.max_connections
      max_pending_requests  = 100
      max_requests          = 10000
      max_retries           = 3
      connect_timeout_ms    = 5000
      request_timeout_ms    = 30000
      health_check_grace_period = "10s"
    }
  })
}

resource "consul_config_entry" "service_resolver" {
  kind = "service-resolver"
  name = "latticedb"

  config_json = jsonencode({
    default_subset = "v1"
    subsets = {
      "v1" = {
        filter = "Service.Meta.version == v1"
      }
      "canary" = {
        filter = "Service.Meta.version == canary"
      }
    }
    load_balancer = {
      policy = "least_request"
      least_request_config = {
        choice_count = 2
      }
    }
    connect_timeout = "5s"
    request_timeout = "30s"
  })
}

resource "consul_config_entry" "service_splitter" {
  count = var.enable_canary_deployments ? 1 : 0

  kind = "service-splitter"
  name = "latticedb"

  config_json = jsonencode({
    splits = [
      {
        weight         = var.canary_weight
        service_subset = "canary"
      },
      {
        weight         = 100 - var.canary_weight
        service_subset = "v1"
      }
    ]
  })
}

# Service intentions for security
resource "consul_config_entry" "intentions" {
  kind = "service-intentions"
  name = "latticedb"

  config_json = jsonencode({
    sources = [
      {
        name   = "web"
        action = "allow"
      },
      {
        name   = "api-gateway"
        action = "allow"
      },
      {
        name   = "monitoring"
        action = "allow"
      },
      {
        name   = "*"
        action = "deny"
      }
    ]
  })
}

# Nomad job specification
resource "nomad_job" "latticedb" {
  jobspec = templatefile("${path.module}/../nomad/latticedb.nomad.hcl", {
    # Variables for template substitution
    datacenter      = var.consul_datacenter
    region          = var.nomad_region
    instance_count  = var.instance_count
    cpu_limit       = var.cpu_limit
    memory_limit    = var.memory_limit
    image           = var.container_image
    version         = var.image_tag
    environment     = var.environment
    cluster_name    = local.cluster_name
    enable_tls      = var.enable_tls
    enable_backup   = var.enable_backup
    backup_interval = var.backup_interval
  })

  hcl2 {
    enabled = true
    vars = {
      datacenter      = var.consul_datacenter
      region          = var.nomad_region
      instance_count  = var.instance_count
      cpu_limit       = var.cpu_limit
      memory_limit    = var.memory_limit
      image           = var.container_image
      version         = var.image_tag
      environment     = var.environment
      cluster_name    = local.cluster_name
      enable_tls      = var.enable_tls
      enable_backup   = var.enable_backup
      backup_interval = var.backup_interval
    }
  }

  depends_on = [
    vault_kv_secret_v2.config,
    vault_kv_secret_v2.database,
    vault_kv_secret_v2.ssl,
    consul_key_prefix.latticedb,
    vault_policy.latticedb
  ]
}

# Monitoring and observability
resource "consul_config_entry" "ingress_gateway" {
  count = var.enable_ingress_gateway ? 1 : 0

  kind = "ingress-gateway"
  name = "latticedb-gateway"

  config_json = jsonencode({
    listeners = [
      {
        port     = 8080
        protocol = "http"
        services = [
          {
            name = "latticedb"
            hosts = var.ingress_hosts
          }
        ]
      },
      {
        port     = 5432
        protocol = "tcp"
        services = [
          {
            name = "latticedb-sql"
          }
        ]
      }
    ]
  })
}

# CSI volume for persistent storage
resource "nomad_csi_volume" "latticedb_data" {
  count     = var.enable_persistent_storage ? 1 : 0
  plugin_id = var.csi_plugin_id
  volume_id = "${local.cluster_name}-data"
  name      = "${local.cluster_name}-data"

  capacity_min = "${var.storage_size_min}GiB"
  capacity_max = "${var.storage_size_max}GiB"

  capability {
    access_mode     = "multi-node-multi-writer"
    attachment_mode = "file-system"
  }

  mount_options {
    fs_type = "ext4"
  }

  topology_request {
    required {
      topology {
        segments = {
          "topology.csi.aws.com/zone" = var.availability_zones[0]
        }
      }
    }
  }
}

# Vault audit configuration
resource "vault_audit" "file" {
  type = "file"

  options = {
    file_path = "/opt/vault/logs/audit.log"
    log_raw   = "false"
  }
}