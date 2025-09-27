output "load_balancer_id" {
  description = "ID of the load balancer"
  value       = aws_lb.main.id
}

output "load_balancer_arn" {
  description = "ARN of the load balancer"
  value       = aws_lb.main.arn
}

output "load_balancer_arn_suffix" {
  description = "ARN suffix for use with CloudWatch Metrics"
  value       = aws_lb.main.arn_suffix
}

output "load_balancer_dns_name" {
  description = "DNS name of the load balancer"
  value       = aws_lb.main.dns_name
}

output "load_balancer_zone_id" {
  description = "Canonical hosted zone ID of the load balancer"
  value       = aws_lb.main.zone_id
}

output "target_group_id" {
  description = "ID of the target group"
  value       = aws_lb_target_group.main.id
}

output "target_group_arn" {
  description = "ARN of the target group"
  value       = aws_lb_target_group.main.arn
}

output "target_group_arn_suffix" {
  description = "ARN suffix for use with CloudWatch Metrics"
  value       = aws_lb_target_group.main.arn_suffix
}

output "target_group_name" {
  description = "Name of the target group"
  value       = aws_lb_target_group.main.name
}

output "blue_green_target_group_arn" {
  description = "ARN of the blue/green target group"
  value       = var.enable_blue_green_deployment ? aws_lb_target_group.blue_green[0].arn : null
}

output "blue_green_target_group_name" {
  description = "Name of the blue/green target group"
  value       = var.enable_blue_green_deployment ? aws_lb_target_group.blue_green[0].name : null
}

output "http_listener_arn" {
  description = "ARN of the HTTP listener"
  value       = var.enable_http_listener ? aws_lb_listener.http[0].arn : null
}

output "https_listener_arn" {
  description = "ARN of the HTTPS listener"
  value       = var.enable_https_listener ? aws_lb_listener.https[0].arn : null
}

output "custom_listener_arn" {
  description = "ARN of the custom listener"
  value       = var.enable_custom_listener ? aws_lb_listener.custom[0].arn : null
}

output "api_listener_rule_arn" {
  description = "ARN of the API listener rule"
  value       = var.enable_api_routing ? aws_lb_listener_rule.api[0].arn : null
}

output "host_listener_rule_arn" {
  description = "ARN of the host listener rule"
  value       = var.enable_host_routing ? aws_lb_listener_rule.host[0].arn : null
}

output "waf_acl_association_id" {
  description = "ID of the WAF ACL association"
  value       = var.enable_waf ? aws_wafv2_web_acl_association.main[0].id : null
}

output "cloudwatch_alarm_response_time_arn" {
  description = "ARN of the response time CloudWatch alarm"
  value       = var.enable_cloudwatch_alarms ? aws_cloudwatch_metric_alarm.target_response_time[0].arn : null
}

output "cloudwatch_alarm_unhealthy_hosts_arn" {
  description = "ARN of the unhealthy hosts CloudWatch alarm"
  value       = var.enable_cloudwatch_alarms ? aws_cloudwatch_metric_alarm.unhealthy_hosts[0].arn : null
}

output "cloudwatch_alarm_http_5xx_arn" {
  description = "ARN of the HTTP 5XX errors CloudWatch alarm"
  value       = var.enable_cloudwatch_alarms ? aws_cloudwatch_metric_alarm.http_5xx_errors[0].arn : null
}

output "route53_health_check_id" {
  description = "ID of the Route53 health check"
  value       = var.enable_route53_health_check ? aws_route53_health_check.main[0].id : null
}

output "security_groups" {
  description = "Security groups attached to the load balancer"
  value       = var.security_group_ids
}

output "subnet_ids" {
  description = "Subnet IDs where the load balancer is deployed"
  value       = var.subnet_ids
}

output "load_balancer_type" {
  description = "Type of the load balancer"
  value       = "application"
}

output "load_balancer_scheme" {
  description = "Load balancer scheme (internal or internet-facing)"
  value       = var.internal ? "internal" : "internet-facing"
}

output "target_group_health_check_path" {
  description = "Health check path for the target group"
  value       = var.health_check_path
}

output "target_group_protocol" {
  description = "Protocol used by the target group"
  value       = var.target_protocol
}

output "target_group_port" {
  description = "Port used by the target group"
  value       = var.target_port
}

output "ssl_policy" {
  description = "SSL policy used by HTTPS listeners"
  value       = var.enable_https_listener || var.custom_listener_protocol == "HTTPS" ? var.ssl_policy : null
}

output "certificate_arn" {
  description = "ARN of the SSL certificate used by HTTPS listeners"
  value       = var.enable_https_listener || var.custom_listener_protocol == "HTTPS" ? var.certificate_arn : null
}

output "access_logs_enabled" {
  description = "Whether access logs are enabled"
  value       = var.enable_access_logs
}

output "access_logs_bucket" {
  description = "S3 bucket for access logs"
  value       = var.enable_access_logs ? var.access_logs_bucket : null
}

output "deletion_protection_enabled" {
  description = "Whether deletion protection is enabled"
  value       = var.enable_deletion_protection
}

output "stickiness_enabled" {
  description = "Whether session stickiness is enabled"
  value       = var.enable_stickiness
}

output "health_check_configuration" {
  description = "Health check configuration"
  value = {
    path                = var.health_check_path
    port                = var.health_check_port
    protocol            = var.health_check_protocol
    healthy_threshold   = var.health_check_healthy_threshold
    unhealthy_threshold = var.health_check_unhealthy_threshold
    timeout             = var.health_check_timeout
    interval            = var.health_check_interval
    matcher             = var.health_check_matcher
  }
}