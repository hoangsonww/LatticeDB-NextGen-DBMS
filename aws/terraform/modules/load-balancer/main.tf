terraform {
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

# Application Load Balancer
resource "aws_lb" "main" {
  name               = var.alb_name
  internal           = var.internal
  load_balancer_type = "application"
  security_groups    = var.security_group_ids
  subnets            = var.subnet_ids

  enable_deletion_protection = var.enable_deletion_protection

  dynamic "access_logs" {
    for_each = var.enable_access_logs ? [1] : []
    content {
      bucket  = var.access_logs_bucket
      prefix  = var.access_logs_prefix
      enabled = true
    }
  }

  tags = var.tags
}

# Target Group
resource "aws_lb_target_group" "main" {
  name        = var.target_group_name
  port        = var.target_port
  protocol    = var.target_protocol
  vpc_id      = var.vpc_id
  target_type = var.target_type

  health_check {
    enabled             = true
    healthy_threshold   = var.health_check_healthy_threshold
    unhealthy_threshold = var.health_check_unhealthy_threshold
    timeout             = var.health_check_timeout
    interval            = var.health_check_interval
    path                = var.health_check_path
    matcher             = var.health_check_matcher
    port                = var.health_check_port
    protocol            = var.health_check_protocol
  }

  stickiness {
    type            = "lb_cookie"
    cookie_duration = var.stickiness_duration
    enabled         = var.enable_stickiness
  }

  tags = var.tags

  lifecycle {
    create_before_destroy = true
  }
}

# HTTP Listener (redirect to HTTPS)
resource "aws_lb_listener" "http" {
  count = var.enable_http_listener ? 1 : 0

  load_balancer_arn = aws_lb.main.arn
  port              = "80"
  protocol          = "HTTP"

  default_action {
    type = "redirect"

    redirect {
      port        = "443"
      protocol    = "HTTPS"
      status_code = "HTTP_301"
    }
  }

  tags = var.tags
}

# HTTPS Listener
resource "aws_lb_listener" "https" {
  count = var.enable_https_listener ? 1 : 0

  load_balancer_arn = aws_lb.main.arn
  port              = "443"
  protocol          = "HTTPS"
  ssl_policy        = var.ssl_policy
  certificate_arn   = var.certificate_arn

  default_action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.main.arn
  }

  tags = var.tags
}

# Custom Port Listener (for database or custom protocols)
resource "aws_lb_listener" "custom" {
  count = var.enable_custom_listener ? 1 : 0

  load_balancer_arn = aws_lb.main.arn
  port              = var.custom_listener_port
  protocol          = var.custom_listener_protocol
  certificate_arn   = var.custom_listener_protocol == "HTTPS" ? var.certificate_arn : null
  ssl_policy        = var.custom_listener_protocol == "HTTPS" ? var.ssl_policy : null

  default_action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.main.arn
  }

  tags = var.tags
}

# Additional Target Groups for blue/green deployments
resource "aws_lb_target_group" "blue_green" {
  count = var.enable_blue_green_deployment ? 1 : 0

  name        = "${var.target_group_name}-bg"
  port        = var.target_port
  protocol    = var.target_protocol
  vpc_id      = var.vpc_id
  target_type = var.target_type

  health_check {
    enabled             = true
    healthy_threshold   = var.health_check_healthy_threshold
    unhealthy_threshold = var.health_check_unhealthy_threshold
    timeout             = var.health_check_timeout
    interval            = var.health_check_interval
    path                = var.health_check_path
    matcher             = var.health_check_matcher
    port                = var.health_check_port
    protocol            = var.health_check_protocol
  }

  stickiness {
    type            = "lb_cookie"
    cookie_duration = var.stickiness_duration
    enabled         = var.enable_stickiness
  }

  tags = merge(var.tags, {
    Name = "${var.target_group_name}-blue-green"
  })

  lifecycle {
    create_before_destroy = true
  }
}

# Listener Rules for path-based routing
resource "aws_lb_listener_rule" "api" {
  count = var.enable_api_routing ? 1 : 0

  listener_arn = var.enable_https_listener ? aws_lb_listener.https[0].arn : aws_lb_listener.custom[0].arn
  priority     = var.api_rule_priority

  action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.main.arn
  }

  condition {
    path_pattern {
      values = var.api_path_patterns
    }
  }

  tags = var.tags
}

# Listener Rules for host-based routing
resource "aws_lb_listener_rule" "host" {
  count = var.enable_host_routing ? 1 : 0

  listener_arn = var.enable_https_listener ? aws_lb_listener.https[0].arn : aws_lb_listener.custom[0].arn
  priority     = var.host_rule_priority

  action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.main.arn
  }

  condition {
    host_header {
      values = var.host_header_values
    }
  }

  tags = var.tags
}

# WAF Web ACL Association
resource "aws_wafv2_web_acl_association" "main" {
  count = var.enable_waf ? 1 : 0

  resource_arn = aws_lb.main.arn
  web_acl_arn  = var.waf_acl_arn
}

# CloudWatch Alarms for Load Balancer
resource "aws_cloudwatch_metric_alarm" "target_response_time" {
  count = var.enable_cloudwatch_alarms ? 1 : 0

  alarm_name          = "${var.alb_name}-high-response-time"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "TargetResponseTime"
  namespace           = "AWS/ApplicationELB"
  period              = "60"
  statistic           = "Average"
  threshold           = var.response_time_threshold
  alarm_description   = "This metric monitors ALB target response time"
  alarm_actions       = var.alarm_actions

  dimensions = {
    LoadBalancer = aws_lb.main.arn_suffix
  }

  tags = var.tags
}

resource "aws_cloudwatch_metric_alarm" "unhealthy_hosts" {
  count = var.enable_cloudwatch_alarms ? 1 : 0

  alarm_name          = "${var.alb_name}-unhealthy-hosts"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "UnHealthyHostCount"
  namespace           = "AWS/ApplicationELB"
  period              = "60"
  statistic           = "Average"
  threshold           = "0"
  alarm_description   = "This metric monitors unhealthy ALB targets"
  alarm_actions       = var.alarm_actions

  dimensions = {
    TargetGroup  = aws_lb_target_group.main.arn_suffix
    LoadBalancer = aws_lb.main.arn_suffix
  }

  tags = var.tags
}

resource "aws_cloudwatch_metric_alarm" "http_5xx_errors" {
  count = var.enable_cloudwatch_alarms ? 1 : 0

  alarm_name          = "${var.alb_name}-http-5xx-errors"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "HTTPCode_Target_5XX_Count"
  namespace           = "AWS/ApplicationELB"
  period              = "60"
  statistic           = "Sum"
  threshold           = var.http_5xx_threshold
  alarm_description   = "This metric monitors 5XX errors from ALB targets"
  alarm_actions       = var.alarm_actions

  dimensions = {
    TargetGroup  = aws_lb_target_group.main.arn_suffix
    LoadBalancer = aws_lb.main.arn_suffix
  }

  tags = var.tags
}

# Route53 Health Check (optional)
resource "aws_route53_health_check" "main" {
  count = var.enable_route53_health_check ? 1 : 0

  fqdn                            = var.health_check_fqdn
  port                            = var.health_check_port
  type                            = "HTTPS"
  resource_path                   = var.health_check_path
  failure_threshold               = var.route53_failure_threshold
  request_interval                = var.route53_request_interval
  cloudwatch_alarm_region         = data.aws_region.current.name
  cloudwatch_alarm_name           = var.enable_cloudwatch_alarms ? aws_cloudwatch_metric_alarm.target_response_time[0].alarm_name : null
  insufficient_data_health_status = "Failure"

  tags = var.tags
}

# Data source for current region
data "aws_region" "current" {}