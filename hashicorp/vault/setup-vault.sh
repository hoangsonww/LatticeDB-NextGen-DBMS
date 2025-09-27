#!/bin/bash

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VAULT_ADDR="${VAULT_ADDR:-http://127.0.0.1:8200}"
ENVIRONMENT="${ENVIRONMENT:-production}"

# Functions
log() {
    echo -e "${BLUE}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} ✅ $1"
}

log_warning() {
    echo -e "${YELLOW}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} ⚠️  $1"
}

log_error() {
    echo -e "${RED}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} ❌ $1"
}

check_vault_status() {
    log "Checking Vault status..."

    if ! vault status &>/dev/null; then
        log_error "Vault is not accessible. Please ensure Vault is running and VAULT_ADDR is set correctly."
        exit 1
    fi

    if vault status | grep -q "Sealed.*true"; then
        log_error "Vault is sealed. Please unseal Vault first."
        exit 1
    fi

    log_success "Vault is accessible and unsealed"
}

check_vault_auth() {
    log "Checking Vault authentication..."

    if ! vault auth -method=token token="$VAULT_TOKEN" &>/dev/null; then
        log_error "Vault authentication failed. Please ensure VAULT_TOKEN is set correctly."
        exit 1
    fi

    log_success "Vault authentication successful"
}

enable_secret_engines() {
    log "Enabling secret engines..."

    # Enable KV v2 secrets engine
    if ! vault secrets list | grep -q "secret/"; then
        vault secrets enable -path=secret kv-v2
        log_success "Enabled KV v2 secrets engine at secret/"
    else
        log_warning "KV v2 secrets engine already enabled at secret/"
    fi

    # Enable PKI engine for service mesh certificates
    if ! vault secrets list | grep -q "pki/"; then
        vault secrets enable -path=pki -max-lease-ttl=87600h pki
        log_success "Enabled PKI engine at pki/"
    else
        log_warning "PKI engine already enabled at pki/"
    fi

    # Enable intermediate PKI
    if ! vault secrets list | grep -q "pki_int/"; then
        vault secrets enable -path=pki_int -max-lease-ttl=8760h pki
        log_success "Enabled intermediate PKI engine at pki_int/"
    else
        log_warning "Intermediate PKI engine already enabled at pki_int/"
    fi

    # Enable database engine
    if ! vault secrets list | grep -q "database/"; then
        vault secrets enable database
        log_success "Enabled database secrets engine"
    else
        log_warning "Database secrets engine already enabled"
    fi

    # Enable transit engine for encryption
    if ! vault secrets list | grep -q "transit/"; then
        vault secrets enable transit
        log_success "Enabled transit encryption engine"
    else
        log_warning "Transit encryption engine already enabled"
    fi

    # Enable Consul secrets engine
    if ! vault secrets list | grep -q "consul/"; then
        vault secrets enable consul
        log_success "Enabled Consul secrets engine"
    else
        log_warning "Consul secrets engine already enabled"
    fi

    # Enable Nomad secrets engine
    if ! vault secrets list | grep -q "nomad/"; then
        vault secrets enable nomad
        log_success "Enabled Nomad secrets engine"
    else
        log_warning "Nomad secrets engine already enabled"
    fi
}

configure_pki() {
    log "Configuring PKI engines..."

    # Generate root CA
    if ! vault read -field=certificate pki/cert/ca &>/dev/null; then
        vault write pki/root/generate/internal \
            common_name="LatticeDB Root CA" \
            issuer_name="root-2024" \
            ttl=87600h
        log_success "Generated root CA certificate"
    else
        log_warning "Root CA certificate already exists"
    fi

    # Configure CA and CRL URLs
    vault write pki/config/urls \
        issuing_certificates="${VAULT_ADDR}/v1/pki/ca" \
        crl_distribution_points="${VAULT_ADDR}/v1/pki/crl"

    # Generate intermediate CA
    if ! vault read -field=certificate pki_int/cert/ca &>/dev/null; then
        vault write -format=json pki_int/intermediate/generate/internal \
            common_name="LatticeDB Intermediate Authority" \
            issuer_name="latticedb-intermediate" \
            | jq -r '.data.csr' > pki_intermediate.csr

        vault write -format=json pki/root/sign-intermediate \
            issuer_ref="root-2024" \
            csr=@pki_intermediate.csr \
            format=pem_bundle ttl="43800h" \
            | jq -r '.data.certificate' > intermediate.cert.pem

        vault write pki_int/intermediate/set-signed certificate=@intermediate.cert.pem

        rm pki_intermediate.csr intermediate.cert.pem
        log_success "Generated and signed intermediate CA certificate"
    else
        log_warning "Intermediate CA certificate already exists"
    fi

    # Configure intermediate CA URLs
    vault write pki_int/config/urls \
        issuing_certificates="${VAULT_ADDR}/v1/pki_int/ca" \
        crl_distribution_points="${VAULT_ADDR}/v1/pki_int/crl"

    # Create a role for LatticeDB services
    vault write pki_int/roles/latticedb \
        issuer_ref="$(vault read -field=default pki_int/config/issuers)" \
        allowed_domains="latticedb.service.consul,localhost" \
        allow_subdomains=true \
        max_ttl="720h" \
        generate_lease=true

    log_success "Configured PKI role for LatticeDB"
}

configure_database() {
    log "Configuring database secrets engine..."

    # Configure PostgreSQL connection (example)
    vault write database/config/latticedb-postgres \
        plugin_name=postgresql-database-plugin \
        connection_url="postgresql://{{username}}:{{password}}@localhost:5432/latticedb?sslmode=disable" \
        allowed_roles="latticedb-role" \
        username="vault" \
        password="vault-password"

    # Create a role for dynamic credentials
    vault write database/roles/latticedb-role \
        db_name=latticedb-postgres \
        creation_statements="CREATE ROLE \"{{name}}\" WITH LOGIN PASSWORD '{{password}}' VALID UNTIL '{{expiration}}'; \
                           GRANT SELECT ON ALL TABLES IN SCHEMA public TO \"{{name}}\";" \
        default_ttl="1h" \
        max_ttl="24h"

    log_success "Configured database dynamic credentials"
}

configure_transit() {
    log "Configuring transit encryption..."

    # Create encryption key for LatticeDB
    if ! vault read transit/keys/latticedb &>/dev/null; then
        vault write -f transit/keys/latticedb
        log_success "Created transit encryption key for LatticeDB"
    else
        log_warning "Transit encryption key already exists"
    fi

    # Enable auto-rotation
    vault write transit/keys/latticedb/config \
        auto_rotate_period=2160h

    log_success "Configured transit encryption auto-rotation"
}

create_policies() {
    log "Creating Vault policies..."

    # Create LatticeDB policy
    vault policy write latticedb-policy "$SCRIPT_DIR/policies.hcl"
    log_success "Created LatticeDB policy"

    # Create admin policy
    vault policy write latticedb-admin-policy - <<EOF
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
EOF

    log_success "Created LatticeDB admin policy"
}

setup_auth_methods() {
    log "Setting up authentication methods..."

    # Enable Kubernetes auth method (if in K8s environment)
    if command -v kubectl &>/dev/null && kubectl cluster-info &>/dev/null; then
        if ! vault auth list | grep -q "kubernetes/"; then
            vault auth enable kubernetes

            # Configure Kubernetes auth
            vault write auth/kubernetes/config \
                kubernetes_host="https://$KUBERNETES_SERVICE_HOST:$KUBERNETES_SERVICE_PORT" \
                kubernetes_ca_cert=@/var/run/secrets/kubernetes.io/serviceaccount/ca.crt \
                token_reviewer_jwt=@/var/run/secrets/kubernetes.io/serviceaccount/token

            # Create role for LatticeDB
            vault write auth/kubernetes/role/latticedb \
                bound_service_account_names=latticedb \
                bound_service_account_namespaces=default,latticedb \
                policies=latticedb-policy \
                ttl=24h

            log_success "Configured Kubernetes authentication"
        else
            log_warning "Kubernetes auth method already enabled"
        fi
    fi

    # Enable AppRole auth method
    if ! vault auth list | grep -q "approle/"; then
        vault auth enable approle

        # Create role for LatticeDB
        vault write auth/approle/role/latticedb \
            token_policies="latticedb-policy" \
            token_ttl=1h \
            token_max_ttl=4h \
            bind_secret_id=true

        log_success "Configured AppRole authentication"
    else
        log_warning "AppRole auth method already enabled"
    fi
}

create_secrets() {
    log "Creating initial secrets..."

    # Create database secrets
    vault kv put secret/latticedb/$ENVIRONMENT/database \
        password="$(openssl rand -base64 32)" \
        admin_password="$(openssl rand -base64 32)" \
        encryption_key="$(openssl rand -base64 32)"

    # Create application configuration
    vault kv put secret/latticedb/$ENVIRONMENT/config \
        environment="$ENVIRONMENT" \
        log_level="info" \
        cluster_name="latticedb-$ENVIRONMENT" \
        http_port="8080" \
        sql_port="5432" \
        max_connections="1000" \
        backup_enabled="true" \
        backup_interval="1h"

    # Create SSL configuration
    vault kv put secret/latticedb/$ENVIRONMENT/ssl \
        tls_enabled="true" \
        tls_min_version="1.2" \
        cipher_suites="ECDHE-RSA-AES256-GCM-SHA384,ECDHE-RSA-AES128-GCM-SHA256"

    log_success "Created initial secrets for environment: $ENVIRONMENT"
}

configure_audit() {
    log "Configuring audit logging..."

    # Enable file audit backend
    if ! vault audit list | grep -q "file/"; then
        vault audit enable file file_path=/opt/vault/logs/audit.log
        log_success "Enabled file audit logging"
    else
        log_warning "File audit logging already enabled"
    fi
}

show_info() {
    log "Vault setup complete! Here's the summary:"
    echo ""
    echo "📊 Secret Engines:"
    echo "   • KV v2 at secret/"
    echo "   • PKI at pki/ and pki_int/"
    echo "   • Database at database/"
    echo "   • Transit at transit/"
    echo "   • Consul at consul/"
    echo "   • Nomad at nomad/"
    echo ""
    echo "🔐 Authentication Methods:"
    vault auth list | grep -E "(approle|kubernetes)" || echo "   • Token authentication (default)"
    echo ""
    echo "📋 Policies Created:"
    echo "   • latticedb-policy"
    echo "   • latticedb-admin-policy"
    echo ""
    echo "🔑 Example Commands:"
    echo "   # Get database password"
    echo "   vault kv get -field=password secret/latticedb/$ENVIRONMENT/database"
    echo ""
    echo "   # Issue a certificate"
    echo "   vault write pki_int/issue/latticedb common_name=latticedb.service.consul"
    echo ""
    echo "   # Get dynamic database credentials"
    echo "   vault read database/creds/latticedb-role"
    echo ""
    echo "   # Encrypt data"
    echo "   vault write transit/encrypt/latticedb plaintext=\$(base64 <<< \"sensitive data\")"
    echo ""
}

main() {
    log "Setting up Vault for LatticeDB deployment..."

    check_vault_status
    check_vault_auth
    enable_secret_engines
    configure_pki
    configure_database
    configure_transit
    create_policies
    setup_auth_methods
    create_secrets
    configure_audit
    show_info

    log_success "Vault setup completed successfully!"
}

# Check required environment variables
if [[ -z "${VAULT_TOKEN:-}" ]]; then
    log_error "VAULT_TOKEN environment variable is required"
    exit 1
fi

main "$@"