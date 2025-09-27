# Monitoring Infrastructure - Prometheus & Grafana for AWS

# CloudWatch Log Groups for Monitoring
resource "aws_cloudwatch_log_group" "prometheus" {
  count             = var.enable_monitoring ? 1 : 0
  name              = "/ecs/${local.cluster_name}-prometheus"
  retention_in_days = var.log_retention_days
  tags              = local.common_tags
}

resource "aws_cloudwatch_log_group" "grafana" {
  count             = var.enable_monitoring ? 1 : 0
  name              = "/ecs/${local.cluster_name}-grafana"
  retention_in_days = var.log_retention_days
  tags              = local.common_tags
}

# EFS for Monitoring Data
resource "aws_efs_file_system" "monitoring" {
  count           = var.enable_monitoring ? 1 : 0
  creation_token  = "${local.cluster_name}-monitoring"
  performance_mode = "generalPurpose"
  throughput_mode = "provisioned"
  provisioned_throughput_in_mibps = 100

  encrypted = true

  tags = merge(local.common_tags, {
    Name = "${local.cluster_name}-monitoring-efs"
  })
}

# EFS Mount Targets for Monitoring
resource "aws_efs_mount_target" "monitoring" {
  count           = var.enable_monitoring ? length(aws_subnet.private) : 0
  file_system_id  = aws_efs_file_system.monitoring[0].id
  subnet_id       = aws_subnet.private[count.index].id
  security_groups = [aws_security_group.efs[0].id]
}

# Security Group for Monitoring Services
resource "aws_security_group" "monitoring" {
  count       = var.enable_monitoring ? 1 : 0
  name        = "${local.cluster_name}-monitoring"
  description = "Security group for monitoring services"
  vpc_id      = aws_vpc.main.id

  ingress {
    from_port   = 9090
    to_port     = 9090
    protocol    = "tcp"
    cidr_blocks = [var.vpc_cidr]
    description = "Prometheus metrics"
  }

  ingress {
    from_port   = 3000
    to_port     = 3000
    protocol    = "tcp"
    cidr_blocks = [var.vpc_cidr]
    description = "Grafana dashboard"
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
    description = "All outbound traffic"
  }

  tags = merge(local.common_tags, {
    Name = "${local.cluster_name}-monitoring"
  })
}

# ECS Task Definition - Prometheus
resource "aws_ecs_task_definition" "prometheus" {
  count                    = var.enable_monitoring ? 1 : 0
  family                   = "${local.cluster_name}-prometheus"
  network_mode             = "awsvpc"
  requires_compatibilities = ["FARGATE"]
  cpu                      = var.prometheus_cpu
  memory                   = var.prometheus_memory
  execution_role_arn       = aws_iam_role.ecs_task_execution.arn
  task_role_arn           = aws_iam_role.ecs_task.arn

  container_definitions = jsonencode([
    {
      name  = "prometheus"
      image = "prom/prometheus:v2.40.0"

      portMappings = [
        {
          containerPort = 9090
          protocol      = "tcp"
        }
      ]

      command = [
        "--config.file=/etc/prometheus/prometheus.yml",
        "--storage.tsdb.path=/prometheus",
        "--storage.tsdb.retention.time=${var.prometheus_retention}",
        "--web.console.libraries=/etc/prometheus/console_libraries",
        "--web.console.templates=/etc/prometheus/consoles",
        "--web.enable-lifecycle",
        "--web.enable-admin-api"
      ]

      environment = [
        {
          name  = "LATTICEDB_CLUSTER"
          value = local.cluster_name
        }
      ]

      logConfiguration = {
        logDriver = "awslogs"
        options = {
          awslogs-group         = aws_cloudwatch_log_group.prometheus[0].name
          awslogs-region        = var.aws_region
          awslogs-stream-prefix = "ecs"
        }
      }

      healthCheck = {
        command     = ["CMD-SHELL", "wget --no-verbose --tries=1 --spider http://localhost:9090/-/healthy || exit 1"]
        interval    = 30
        timeout     = 5
        retries     = 3
        startPeriod = 30
      }

      mountPoints = [
        {
          sourceVolume  = "prometheus-config"
          containerPath = "/etc/prometheus"
          readOnly     = false
        },
        {
          sourceVolume  = "prometheus-data"
          containerPath = "/prometheus"
          readOnly     = false
        }
      ]

      essential = true
    }
  ])

  volume {
    name = "prometheus-config"
    efs_volume_configuration {
      file_system_id = aws_efs_file_system.monitoring[0].id
      root_directory = "/prometheus-config"
    }
  }

  volume {
    name = "prometheus-data"
    efs_volume_configuration {
      file_system_id = aws_efs_file_system.monitoring[0].id
      root_directory = "/prometheus-data"
    }
  }

  tags = local.common_tags
}

# ECS Service - Prometheus
resource "aws_ecs_service" "prometheus" {
  count           = var.enable_monitoring ? 1 : 0
  name            = "${local.cluster_name}-prometheus"
  cluster         = aws_ecs_cluster.main.id
  task_definition = aws_ecs_task_definition.prometheus[0].arn
  desired_count   = 1
  launch_type     = "FARGATE"

  network_configuration {
    security_groups  = [aws_security_group.monitoring[0].id]
    subnets          = aws_subnet.private[*].id
    assign_public_ip = false
  }

  service_registries {
    registry_arn = aws_service_discovery_service.prometheus[0].arn
  }

  tags = local.common_tags
}

# ECS Task Definition - Grafana
resource "aws_ecs_task_definition" "grafana" {
  count                    = var.enable_monitoring ? 1 : 0
  family                   = "${local.cluster_name}-grafana"
  network_mode             = "awsvpc"
  requires_compatibilities = ["FARGATE"]
  cpu                      = var.grafana_cpu
  memory                   = var.grafana_memory
  execution_role_arn       = aws_iam_role.ecs_task_execution.arn
  task_role_arn           = aws_iam_role.ecs_task.arn

  container_definitions = jsonencode([
    {
      name  = "grafana"
      image = "grafana/grafana:9.3.0"

      portMappings = [
        {
          containerPort = 3000
          protocol      = "tcp"
        }
      ]

      environment = [
        {
          name  = "GF_SECURITY_ADMIN_USER"
          value = "admin"
        },
        {
          name  = "GF_SECURITY_ADMIN_PASSWORD"
          value = var.grafana_admin_password
        },
        {
          name  = "GF_INSTALL_PLUGINS"
          value = "grafana-piechart-panel,grafana-worldmap-panel,grafana-clock-panel"
        },
        {
          name  = "GF_PATHS_PROVISIONING"
          value = "/etc/grafana/provisioning"
        }
      ]

      logConfiguration = {
        logDriver = "awslogs"
        options = {
          awslogs-group         = aws_cloudwatch_log_group.grafana[0].name
          awslogs-region        = var.aws_region
          awslogs-stream-prefix = "ecs"
        }
      }

      healthCheck = {
        command     = ["CMD-SHELL", "wget --no-verbose --tries=1 --spider http://localhost:3000/api/health || exit 1"]
        interval    = 30
        timeout     = 5
        retries     = 3
        startPeriod = 30
      }

      mountPoints = [
        {
          sourceVolume  = "grafana-data"
          containerPath = "/var/lib/grafana"
          readOnly     = false
        },
        {
          sourceVolume  = "grafana-config"
          containerPath = "/etc/grafana/provisioning"
          readOnly     = false
        }
      ]

      essential = true
    }
  ])

  volume {
    name = "grafana-data"
    efs_volume_configuration {
      file_system_id = aws_efs_file_system.monitoring[0].id
      root_directory = "/grafana-data"
    }
  }

  volume {
    name = "grafana-config"
    efs_volume_configuration {
      file_system_id = aws_efs_file_system.monitoring[0].id
      root_directory = "/grafana-config"
    }
  }

  tags = local.common_tags
}

# ECS Service - Grafana
resource "aws_ecs_service" "grafana" {
  count           = var.enable_monitoring ? 1 : 0
  name            = "${local.cluster_name}-grafana"
  cluster         = aws_ecs_cluster.main.id
  task_definition = aws_ecs_task_definition.grafana[0].arn
  desired_count   = 1
  launch_type     = "FARGATE"

  network_configuration {
    security_groups  = [aws_security_group.monitoring[0].id]
    subnets          = aws_subnet.private[*].id
    assign_public_ip = false
  }

  service_registries {
    registry_arn = aws_service_discovery_service.grafana[0].arn
  }

  tags = local.common_tags
}

# Service Discovery for Monitoring
resource "aws_service_discovery_private_dns_namespace" "monitoring" {
  count       = var.enable_monitoring ? 1 : 0
  name        = "${local.cluster_name}.monitoring"
  description = "Service discovery for monitoring services"
  vpc         = aws_vpc.main.id

  tags = local.common_tags
}

resource "aws_service_discovery_service" "prometheus" {
  count = var.enable_monitoring ? 1 : 0
  name  = "prometheus"

  dns_config {
    namespace_id = aws_service_discovery_private_dns_namespace.monitoring[0].id

    dns_records {
      ttl  = 10
      type = "A"
    }

    routing_policy = "MULTIVALUE"
  }

  health_check_grace_period_seconds = 30
  tags = local.common_tags
}

resource "aws_service_discovery_service" "grafana" {
  count = var.enable_monitoring ? 1 : 0
  name  = "grafana"

  dns_config {
    namespace_id = aws_service_discovery_private_dns_namespace.monitoring[0].id

    dns_records {
      ttl  = 10
      type = "A"
    }

    routing_policy = "MULTIVALUE"
  }

  health_check_grace_period_seconds = 30
  tags = local.common_tags
}

# Load Balancer Target Groups for Monitoring
resource "aws_lb_target_group" "grafana" {
  count       = var.enable_monitoring ? 1 : 0
  name        = "${local.cluster_name}-grafana"
  port        = 3000
  protocol    = "HTTP"
  vpc_id      = aws_vpc.main.id
  target_type = "ip"

  health_check {
    enabled             = true
    healthy_threshold   = 2
    interval            = 30
    matcher             = "200"
    path                = "/api/health"
    port                = "traffic-port"
    protocol            = "HTTP"
    timeout             = 5
    unhealthy_threshold = 2
  }

  tags = local.common_tags
}

# ALB Listener Rule for Grafana
resource "aws_lb_listener_rule" "grafana" {
  count        = var.enable_monitoring ? 1 : 0
  listener_arn = aws_lb_listener.latticedb.arn
  priority     = 100

  action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.grafana[0].arn
  }

  condition {
    path_pattern {
      values = ["/grafana*"]
    }
  }

  tags = local.common_tags
}