output "sns_topic_arn" {
  description = "ARN of the SNS topic for alerts"
  value       = aws_sns_topic.alerts.arn
}

output "sns_topic_name" {
  description = "Name of the SNS topic for alerts"
  value       = aws_sns_topic.alerts.name
}

output "dashboard_name" {
  description = "Name of the CloudWatch dashboard"
  value       = aws_cloudwatch_dashboard.main.dashboard_name
}

output "dashboard_url" {
  description = "URL of the CloudWatch dashboard"
  value       = "https://${data.aws_region.current.name}.console.aws.amazon.com/cloudwatch/home?region=${data.aws_region.current.name}#dashboards:name=${aws_cloudwatch_dashboard.main.dashboard_name}"
}

output "application_log_group_name" {
  description = "Name of the application log group"
  value       = aws_cloudwatch_log_group.application_logs.name
}

output "application_log_group_arn" {
  description = "ARN of the application log group"
  value       = aws_cloudwatch_log_group.application_logs.arn
}

output "ecs_cpu_alarm_arn" {
  description = "ARN of the ECS CPU utilization alarm"
  value       = var.enable_ecs_monitoring ? aws_cloudwatch_metric_alarm.ecs_cpu_high[0].arn : null
}

output "ecs_memory_alarm_arn" {
  description = "ARN of the ECS memory utilization alarm"
  value       = var.enable_ecs_monitoring ? aws_cloudwatch_metric_alarm.ecs_memory_high[0].arn : null
}

output "ecs_service_count_alarm_arn" {
  description = "ARN of the ECS service count alarm"
  value       = var.enable_ecs_monitoring ? aws_cloudwatch_metric_alarm.ecs_service_count_low[0].arn : null
}

output "alb_response_time_alarm_arn" {
  description = "ARN of the ALB response time alarm"
  value       = var.enable_alb_monitoring ? aws_cloudwatch_metric_alarm.alb_response_time_high[0].arn : null
}

output "alb_5xx_errors_alarm_arn" {
  description = "ARN of the ALB 5XX errors alarm"
  value       = var.enable_alb_monitoring ? aws_cloudwatch_metric_alarm.alb_5xx_errors_high[0].arn : null
}

output "alb_unhealthy_hosts_alarm_arn" {
  description = "ARN of the ALB unhealthy hosts alarm"
  value       = var.enable_alb_monitoring ? aws_cloudwatch_metric_alarm.alb_unhealthy_hosts[0].arn : null
}

output "rds_cpu_alarm_arn" {
  description = "ARN of the RDS CPU utilization alarm"
  value       = var.enable_rds_monitoring ? aws_cloudwatch_metric_alarm.rds_cpu_high[0].arn : null
}

output "rds_connections_alarm_arn" {
  description = "ARN of the RDS connections alarm"
  value       = var.enable_rds_monitoring ? aws_cloudwatch_metric_alarm.rds_connections_high[0].arn : null
}

output "rds_storage_alarm_arn" {
  description = "ARN of the RDS storage alarm"
  value       = var.enable_rds_monitoring ? aws_cloudwatch_metric_alarm.rds_storage_low[0].arn : null
}

output "application_errors_alarm_arn" {
  description = "ARN of the application errors alarm"
  value       = var.enable_custom_metrics ? aws_cloudwatch_metric_alarm.application_errors_high[0].arn : null
}

output "ecs_task_state_event_rule_arn" {
  description = "ARN of the ECS task state change event rule"
  value       = var.enable_ecs_monitoring ? aws_cloudwatch_event_rule.ecs_task_state_change[0].arn : null
}

output "xray_sampling_rule_arn" {
  description = "ARN of the X-Ray sampling rule"
  value       = var.enable_xray_tracing ? aws_xray_sampling_rule.main[0].arn : null
}

output "error_metric_filter_name" {
  description = "Name of the error count metric filter"
  value       = var.enable_custom_metrics ? aws_cloudwatch_log_metric_filter.error_count[0].name : null
}

output "top_errors_query_definition_name" {
  description = "Name of the top errors query definition"
  value       = var.enable_custom_metrics ? aws_cloudwatch_query_definition.top_errors[0].name : null
}

output "slow_queries_query_definition_name" {
  description = "Name of the slow queries query definition"
  value       = var.enable_custom_metrics ? aws_cloudwatch_query_definition.slow_queries[0].name : null
}

output "monitoring_configuration" {
  description = "Monitoring configuration summary"
  value = {
    ecs_monitoring_enabled     = var.enable_ecs_monitoring
    alb_monitoring_enabled     = var.enable_alb_monitoring
    rds_monitoring_enabled     = var.enable_rds_monitoring
    custom_metrics_enabled     = var.enable_custom_metrics
    xray_tracing_enabled       = var.enable_xray_tracing
    log_retention_days         = var.log_retention_days
    email_subscriptions_count  = length(var.alert_email_addresses)
    slack_notifications        = var.slack_webhook_url != null
  }
}

output "alarm_thresholds" {
  description = "Alarm thresholds configuration"
  value = {
    ecs_cpu_threshold           = var.ecs_cpu_threshold
    ecs_memory_threshold        = var.ecs_memory_threshold
    ecs_min_running_tasks       = var.ecs_min_running_tasks
    alb_response_time_threshold = var.alb_response_time_threshold
    alb_5xx_threshold          = var.alb_5xx_threshold
    rds_cpu_threshold          = var.rds_cpu_threshold
    rds_connections_threshold   = var.rds_connections_threshold
    rds_free_storage_threshold  = var.rds_free_storage_threshold
    application_error_threshold = var.application_error_threshold
  }
}

output "alert_endpoints" {
  description = "Alert notification endpoints"
  value = {
    sns_topic_arn    = aws_sns_topic.alerts.arn
    email_addresses  = var.alert_email_addresses
    slack_webhook    = var.slack_webhook_url != null ? "configured" : "not_configured"
  }
}

output "cloudwatch_insights_queries" {
  description = "CloudWatch Insights query definitions"
  value = {
    top_errors_query    = var.enable_custom_metrics ? aws_cloudwatch_query_definition.top_errors[0].name : null
    slow_queries_query  = var.enable_custom_metrics ? aws_cloudwatch_query_definition.slow_queries[0].name : null
  }
}

output "logs_configuration" {
  description = "Logging configuration"
  value = {
    application_log_group = aws_cloudwatch_log_group.application_logs.name
    log_retention_days    = var.log_retention_days
    error_log_pattern     = var.error_log_pattern
  }
}