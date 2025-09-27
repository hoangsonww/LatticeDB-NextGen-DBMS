# Vault Policies for LatticeDB

# Main LatticeDB policy
path "secret/data/latticedb/*" {
  capabilities = ["read", "list"]
}

path "secret/metadata/latticedb/*" {
  capabilities = ["read", "list"]
}

# Database secrets
path "secret/data/latticedb/+/database" {
  capabilities = ["read"]
}

# Configuration secrets
path "secret/data/latticedb/+/config" {
  capabilities = ["read"]
}

# SSL/TLS certificates
path "secret/data/latticedb/+/ssl" {
  capabilities = ["read"]
}

# PKI for service mesh certificates
path "pki/issue/latticedb" {
  capabilities = ["create", "update"]
}

path "pki/sign/latticedb" {
  capabilities = ["create", "update"]
}

# Dynamic database credentials
path "database/creds/latticedb-role" {
  capabilities = ["read"]
}

# Transit engine for encryption
path "transit/encrypt/latticedb" {
  capabilities = ["update"]
}

path "transit/decrypt/latticedb" {
  capabilities = ["update"]
}

# Auth methods
path "auth/token/lookup-self" {
  capabilities = ["read"]
}

path "auth/token/renew-self" {
  capabilities = ["update"]
}

# Consul integration
path "consul/creds/latticedb" {
  capabilities = ["read"]
}

# Nomad integration
path "nomad/creds/latticedb" {
  capabilities = ["read"]
}

# Admin policy (for deployment and management)
path "secret/data/latticedb/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

path "secret/metadata/latticedb/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# PKI admin permissions
path "pki/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

path "pki_int/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# Database admin permissions
path "database/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# Transit admin permissions
path "transit/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# System backend access for operators
path "sys/auth" {
  capabilities = ["read"]
}

path "sys/auth/*" {
  capabilities = ["create", "read", "update", "delete"]
}

path "sys/mounts" {
  capabilities = ["read"]
}

path "sys/mounts/*" {
  capabilities = ["create", "read", "update", "delete"]
}

path "sys/policy" {
  capabilities = ["read"]
}

path "sys/policy/*" {
  capabilities = ["create", "read", "update", "delete"]
}

# Audit log access
path "sys/audit" {
  capabilities = ["read"]
}

path "sys/audit/*" {
  capabilities = ["create", "read", "update", "delete"]
}