# ACL Policies for LatticeDB Consul Integration

# LatticeDB service policy
service_prefix "latticedb" {
  policy = "write"
}

service "latticedb" {
  policy = "write"
}

service "latticedb-sql" {
  policy = "write"
}

service "latticedb-admin" {
  policy = "write"
}

# Node policy for service registration
node_prefix "" {
  policy = "read"
}

node_prefix "latticedb" {
  policy = "write"
}

# Key-value store access for configuration
key_prefix "latticedb/" {
  policy = "write"
}

key_prefix "config/latticedb/" {
  policy = "write"
}

# Session management for leader election
session_prefix "" {
  policy = "write"
}

# Event access for cluster coordination
event_prefix "latticedb" {
  policy = "write"
}

# Query access for service discovery
query_prefix "latticedb" {
  policy = "write"
}

# Prepared query for load balancing
prepared_query_prefix "latticedb" {
  policy = "write"
}

# Connect (service mesh) permissions
mesh = "write"
peering = "read"

# Service intentions
intentions = "write"

service "latticedb" {
  intentions = "write"
}

service "latticedb-sql" {
  intentions = "write"
}

# Operator privileges for administrative tasks
operator = "read"

# Agent privileges
agent_prefix "" {
  policy = "read"
}

# ACL management (limited)
acl = "read"