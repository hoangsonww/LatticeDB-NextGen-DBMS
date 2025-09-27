# Consul Connect service mesh configuration for LatticeDB
connect {
  enabled = true
}

# CA configuration for service mesh
ca_config {
  provider = "consul"

  config {
    # Root certificate TTL
    root_cert_ttl = "87600h" # 10 years

    # Intermediate certificate TTL
    intermediate_cert_ttl = "8760h" # 1 year

    # Leaf certificate TTL
    leaf_cert_ttl = "72h" # 3 days

    # Key type and bits
    private_key_type = "ec"
    private_key_bits = 256

    # Rotation period
    rotation_period = "2160h" # 90 days
  }
}

# Service mesh proxy defaults
proxy_defaults {
  protocol = "http"

  config {
    # Connection limits
    max_connections = 1000
    max_pending_requests = 100
    max_requests = 10000
    max_retries = 3

    # Timeout settings
    connect_timeout_ms = 5000
    request_timeout_ms = 30000

    # Health check settings
    health_check_grace_period = "10s"

    # Metrics collection
    envoy_dogstatsd_url = "udp://127.0.0.1:8125"
    envoy_stats_config_json = jsonencode({
      stats_tags = [
        {
          tag_name = "local_cluster"
          regex = "^cluster\\.((.+?)\\.).*"
        }
      ]
    })
  }

  # Upstream defaults
  upstream_config {
    protocol = "http"
    connect_timeout_ms = 5000

    limits {
      max_connections = 100
      max_pending_requests = 10
      max_requests = 1000
    }

    passive_health_check {
      max_failures = 5
      interval = "30s"
      base_ejection_time = "30s"
    }
  }
}

# Service intentions (traffic authorization)
intentions = [
  {
    source_name = "latticedb"
    destination_name = "vault"
    action = "allow"
    description = "Allow LatticeDB to access Vault for secrets"
  },
  {
    source_name = "latticedb"
    destination_name = "consul"
    action = "allow"
    description = "Allow LatticeDB to access Consul for service discovery"
  },
  {
    source_name = "web"
    destination_name = "latticedb"
    action = "allow"
    description = "Allow web services to access LatticeDB HTTP API"
  },
  {
    source_name = "api-gateway"
    destination_name = "latticedb"
    action = "allow"
    description = "Allow API gateway to route traffic to LatticeDB"
  },
  {
    source_name = "monitoring"
    destination_name = "latticedb"
    action = "allow"
    description = "Allow monitoring systems to scrape LatticeDB metrics"
  },
  {
    source_name = "*"
    destination_name = "latticedb"
    action = "deny"
    description = "Deny all other access to LatticeDB by default"
  }
]

# Service resolver for load balancing
service_resolver "latticedb" {
  default_subset = "v1"

  subsets = {
    "v1" = {
      filter = "Service.Meta.version == v1"
    }
    "canary" = {
      filter = "Service.Meta.version == canary"
    }
  }

  load_balancer {
    policy = "least_request"

    least_request_config {
      choice_count = 2
    }
  }

  connect_timeout = "5s"
  request_timeout = "30s"
}

# Service splitter for canary deployments
service_splitter "latticedb" {
  splits = [
    {
      weight = 90
      service_subset = "v1"
    },
    {
      weight = 10
      service_subset = "canary"
    }
  ]
}

# Service router for path-based routing
service_router "latticedb" {
  routes = [
    {
      match {
        http {
          path_prefix = "/api/v1"
          header = [
            {
              name = "x-latticedb-version"
              exact = "v1"
            }
          ]
        }
      }

      destination {
        service = "latticedb"
        service_subset = "v1"

        retry_policy {
          retry_on = "5xx"
          num_retries = 3
          retry_on_connect_failure = true
          retry_on_status_codes = [500, 502, 503, 504]
        }

        timeout = "30s"

        request_headers {
          add = {
            "x-consul-source" = "consul-connect"
          }
        }

        response_headers {
          add = {
            "x-latticedb-node" = "%DOWNSTREAM_REMOTE_ADDRESS%"
          }
        }
      }
    },
    {
      match {
        http {
          path_prefix = "/admin"
        }
      }

      destination {
        service = "latticedb-admin"

        request_headers {
          add = {
            "x-admin-request" = "true"
          }
        }
      }
    }
  ]
}

# Mesh gateway configuration
mesh_gateway {
  mode = "local"
}

# Certificate authority configuration
certificate_authority {
  provider = "vault"

  config {
    address = "http://127.0.0.1:8200"
    token_file = "/etc/consul.d/vault-token"
    root_pki_path = "connect_root"
    intermediate_pki_path = "connect_inter"

    # Additional Vault configuration
    auth_method = {
      type = "kubernetes"
      mount_path = "auth/kubernetes"

      config = {
        role = "consul-ca"
      }
    }
  }
}

# Service mesh observability
observability_tracing {
  enabled = true

  config {
    jaeger {
      collector_endpoint = "http://jaeger-collector:14268/api/traces"
      sampler_type = "probabilistic"
      sampler_param = 0.1
    }
  }
}