terraform {
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

# SNS Topic for Alerts
resource "aws_sns_topic" "alerts" {
  name = var.sns_topic_name

  tags = var.tags
}

resource "aws_sns_topic_policy" "alerts" {
  arn = aws_sns_topic.alerts.arn

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Principal = {
          Service = [
            "cloudwatch.amazonaws.com",
            "events.amazonaws.com"
          ]
        }
        Action   = "sns:Publish"
        Resource = aws_sns_topic.alerts.arn
      }
    ]
  })
}

# SNS Topic Subscriptions
resource "aws_sns_topic_subscription" "email" {
  count = length(var.alert_email_addresses)

  topic_arn = aws_sns_topic.alerts.arn
  protocol  = "email"
  endpoint  = var.alert_email_addresses[count.index]
}

resource "aws_sns_topic_subscription" "slack" {
  count = var.slack_webhook_url != null ? 1 : 0

  topic_arn = aws_sns_topic.alerts.arn
  protocol  = "https"
  endpoint  = var.slack_webhook_url
}

# CloudWatch Dashboard
resource "aws_cloudwatch_dashboard" "main" {
  dashboard_name = var.dashboard_name

  dashboard_body = jsonencode({
    widgets = concat(
      # ECS Service Metrics
      var.enable_ecs_monitoring ? [
        {
          type   = "metric"
          x      = 0
          y      = 0
          width  = 12
          height = 6

          properties = {
            metrics = [
              ["AWS/ECS", "CPUUtilization", "ServiceName", var.ecs_service_name, "ClusterName", var.ecs_cluster_name],
              [".", "MemoryUtilization", ".", ".", ".", "."]
            ]
            view    = "timeSeries"
            stacked = false
            region  = data.aws_region.current.name
            period  = 300
            title   = "ECS Service - CPU and Memory"
          }
        },
        {
          type   = "metric"
          x      = 0
          y      = 6
          width  = 12
          height = 6

          properties = {
            metrics = [
              ["AWS/ECS", "RunningTaskCount", "ServiceName", var.ecs_service_name, "ClusterName", var.ecs_cluster_name],
              [".", "PendingTaskCount", ".", ".", ".", "."],
              [".", "DesiredCount", ".", ".", ".", "."]
            ]
            view    = "timeSeries"
            stacked = false
            region  = data.aws_region.current.name
            period  = 300
            title   = "ECS Service - Task Counts"
          }
        }
      ] : [],

      # Application Load Balancer Metrics
      var.enable_alb_monitoring ? [
        {
          type   = "metric"
          x      = 12
          y      = 0
          width  = 12
          height = 6

          properties = {
            metrics = [
              ["AWS/ApplicationELB", "TargetResponseTime", "LoadBalancer", var.alb_name],
              [".", "RequestCount", ".", "."],
              [".", "HTTPCode_Target_2XX_Count", ".", "."],
              [".", "HTTPCode_Target_4XX_Count", ".", "."],
              [".", "HTTPCode_Target_5XX_Count", ".", "."]
            ]
            view    = "timeSeries"
            stacked = false
            region  = data.aws_region.current.name
            period  = 300
            title   = "ALB - Response Time and Request Counts"
          }
        }
      ] : [],

      # RDS Metrics (if enabled)
      var.enable_rds_monitoring ? [
        {
          type   = "metric"
          x      = 0
          y      = 12
          width  = 12
          height = 6

          properties = {
            metrics = [
              ["AWS/RDS", "CPUUtilization", "DBInstanceIdentifier", var.rds_instance_identifier],
              [".", "DatabaseConnections", ".", "."],
              [".", "FreeStorageSpace", ".", "."]
            ]
            view    = "timeSeries"
            stacked = false
            region  = data.aws_region.current.name
            period  = 300
            title   = "RDS - CPU, Connections, and Storage"
          }
        }
      ] : []
    )
  })
}

# CloudWatch Alarms for ECS Service
resource "aws_cloudwatch_metric_alarm" "ecs_cpu_high" {
  count = var.enable_ecs_monitoring ? 1 : 0

  alarm_name          = "${var.ecs_service_name}-cpu-high"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "CPUUtilization"
  namespace           = "AWS/ECS"
  period              = "300"
  statistic           = "Average"
  threshold           = var.ecs_cpu_threshold
  alarm_description   = "This metric monitors ECS service CPU utilization"
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions         = [aws_sns_topic.alerts.arn]

  dimensions = {
    ServiceName = var.ecs_service_name
    ClusterName = var.ecs_cluster_name
  }

  tags = var.tags
}

resource "aws_cloudwatch_metric_alarm" "ecs_memory_high" {
  count = var.enable_ecs_monitoring ? 1 : 0

  alarm_name          = "${var.ecs_service_name}-memory-high"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "MemoryUtilization"
  namespace           = "AWS/ECS"
  period              = "300"
  statistic           = "Average"
  threshold           = var.ecs_memory_threshold
  alarm_description   = "This metric monitors ECS service memory utilization"
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions         = [aws_sns_topic.alerts.arn]

  dimensions = {
    ServiceName = var.ecs_service_name
    ClusterName = var.ecs_cluster_name
  }

  tags = var.tags
}

resource "aws_cloudwatch_metric_alarm" "ecs_service_count_low" {
  count = var.enable_ecs_monitoring ? 1 : 0

  alarm_name          = "${var.ecs_service_name}-service-count-low"
  comparison_operator = "LessThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "RunningTaskCount"
  namespace           = "AWS/ECS"
  period              = "300"
  statistic           = "Average"
  threshold           = var.ecs_min_running_tasks
  alarm_description   = "This metric monitors ECS service running task count"
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions         = [aws_sns_topic.alerts.arn]

  dimensions = {
    ServiceName = var.ecs_service_name
    ClusterName = var.ecs_cluster_name
  }

  tags = var.tags
}

# CloudWatch Alarms for Application Load Balancer
resource "aws_cloudwatch_metric_alarm" "alb_response_time_high" {
  count = var.enable_alb_monitoring ? 1 : 0

  alarm_name          = "${var.alb_name}-response-time-high"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "TargetResponseTime"
  namespace           = "AWS/ApplicationELB"
  period              = "300"
  statistic           = "Average"
  threshold           = var.alb_response_time_threshold
  alarm_description   = "This metric monitors ALB target response time"
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions         = [aws_sns_topic.alerts.arn]

  dimensions = {
    LoadBalancer = var.alb_name
  }

  tags = var.tags
}

resource "aws_cloudwatch_metric_alarm" "alb_5xx_errors_high" {
  count = var.enable_alb_monitoring ? 1 : 0

  alarm_name          = "${var.alb_name}-5xx-errors-high"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "HTTPCode_Target_5XX_Count"
  namespace           = "AWS/ApplicationELB"
  period              = "300"
  statistic           = "Sum"
  threshold           = var.alb_5xx_threshold
  alarm_description   = "This metric monitors ALB 5XX errors"
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions         = [aws_sns_topic.alerts.arn]

  dimensions = {
    LoadBalancer = var.alb_name
  }

  tags = var.tags
}

resource "aws_cloudwatch_metric_alarm" "alb_unhealthy_hosts" {
  count = var.enable_alb_monitoring ? 1 : 0

  alarm_name          = "${var.alb_name}-unhealthy-hosts"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "UnHealthyHostCount"
  namespace           = "AWS/ApplicationELB"
  period              = "300"
  statistic           = "Average"
  threshold           = "0"
  alarm_description   = "This metric monitors unhealthy ALB targets"
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions         = [aws_sns_topic.alerts.arn]

  dimensions = {
    LoadBalancer  = var.alb_name
    TargetGroup   = var.target_group_name
  }

  tags = var.tags
}

# CloudWatch Alarms for RDS (if enabled)
resource "aws_cloudwatch_metric_alarm" "rds_cpu_high" {
  count = var.enable_rds_monitoring ? 1 : 0

  alarm_name          = "${var.rds_instance_identifier}-cpu-high"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "CPUUtilization"
  namespace           = "AWS/RDS"
  period              = "300"
  statistic           = "Average"
  threshold           = var.rds_cpu_threshold
  alarm_description   = "This metric monitors RDS instance CPU utilization"
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions         = [aws_sns_topic.alerts.arn]

  dimensions = {
    DBInstanceIdentifier = var.rds_instance_identifier
  }

  tags = var.tags
}

resource "aws_cloudwatch_metric_alarm" "rds_connections_high" {
  count = var.enable_rds_monitoring ? 1 : 0

  alarm_name          = "${var.rds_instance_identifier}-connections-high"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "DatabaseConnections"
  namespace           = "AWS/RDS"
  period              = "300"
  statistic           = "Average"
  threshold           = var.rds_connections_threshold
  alarm_description   = "This metric monitors RDS database connections"
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions         = [aws_sns_topic.alerts.arn]

  dimensions = {
    DBInstanceIdentifier = var.rds_instance_identifier
  }

  tags = var.tags
}

resource "aws_cloudwatch_metric_alarm" "rds_storage_low" {
  count = var.enable_rds_monitoring ? 1 : 0

  alarm_name          = "${var.rds_instance_identifier}-storage-low"
  comparison_operator = "LessThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "FreeStorageSpace"
  namespace           = "AWS/RDS"
  period              = "300"
  statistic           = "Average"
  threshold           = var.rds_free_storage_threshold
  alarm_description   = "This metric monitors RDS free storage space"
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions         = [aws_sns_topic.alerts.arn]

  dimensions = {
    DBInstanceIdentifier = var.rds_instance_identifier
  }

  tags = var.tags
}

# Custom Application Metrics
resource "aws_cloudwatch_log_group" "application_logs" {
  name              = "/aws/ecs/${var.ecs_service_name}"
  retention_in_days = var.log_retention_days

  tags = var.tags
}

resource "aws_cloudwatch_log_metric_filter" "error_count" {
  count = var.enable_custom_metrics ? 1 : 0

  name           = "${var.ecs_service_name}-error-count"
  log_group_name = aws_cloudwatch_log_group.application_logs.name
  pattern        = var.error_log_pattern

  metric_transformation {
    name      = "ErrorCount"
    namespace = "LatticeDB/Application"
    value     = "1"
  }
}

resource "aws_cloudwatch_metric_alarm" "application_errors_high" {
  count = var.enable_custom_metrics ? 1 : 0

  alarm_name          = "${var.ecs_service_name}-application-errors-high"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = "2"
  metric_name         = "ErrorCount"
  namespace           = "LatticeDB/Application"
  period              = "300"
  statistic           = "Sum"
  threshold           = var.application_error_threshold
  alarm_description   = "This metric monitors application error count"
  alarm_actions       = [aws_sns_topic.alerts.arn]
  ok_actions         = [aws_sns_topic.alerts.arn]

  tags = var.tags
}

# CloudWatch Events Rule for ECS Task State Changes
resource "aws_cloudwatch_event_rule" "ecs_task_state_change" {
  count = var.enable_ecs_monitoring ? 1 : 0

  name        = "${var.ecs_service_name}-task-state-change"
  description = "Capture ECS task state changes"

  event_pattern = jsonencode({
    source      = ["aws.ecs"]
    detail-type = ["ECS Task State Change"]
    detail = {
      clusterArn = [var.ecs_cluster_arn]
      lastStatus = ["STOPPED"]
    }
  })

  tags = var.tags
}

resource "aws_cloudwatch_event_target" "ecs_task_state_change_sns" {
  count = var.enable_ecs_monitoring ? 1 : 0

  rule      = aws_cloudwatch_event_rule.ecs_task_state_change[0].name
  target_id = "SendToSNS"
  arn       = aws_sns_topic.alerts.arn

  input_transformer {
    input_paths = {
      clusterArn = "$.detail.clusterArn"
      taskArn    = "$.detail.taskArn"
      lastStatus = "$.detail.lastStatus"
      stoppedReason = "$.detail.stoppedReason"
    }
    input_template = "\"ECS Task State Change: Task <taskArn> in cluster <clusterArn> has status <lastStatus>. Reason: <stoppedReason>\""
  }
}

# X-Ray Tracing (optional)
resource "aws_xray_sampling_rule" "main" {
  count = var.enable_xray_tracing ? 1 : 0

  rule_name      = "${var.ecs_service_name}-sampling-rule"
  priority       = 9000
  version        = 1
  reservoir_size = 1
  fixed_rate     = 0.1
  url_path       = "*"
  host           = "*"
  http_method    = "*"
  service_type   = "*"
  service_name   = var.ecs_service_name
  resource_arn   = "*"

  tags = var.tags
}

# CloudWatch Insights Query Definitions
resource "aws_cloudwatch_query_definition" "top_errors" {
  count = var.enable_custom_metrics ? 1 : 0

  name = "${var.ecs_service_name}-top-errors"

  log_group_names = [
    aws_cloudwatch_log_group.application_logs.name
  ]

  query_string = <<EOF
fields @timestamp, @message
| filter @message like /ERROR/
| stats count() by bin(5m)
EOF
}

resource "aws_cloudwatch_query_definition" "slow_queries" {
  count = var.enable_custom_metrics ? 1 : 0

  name = "${var.ecs_service_name}-slow-queries"

  log_group_names = [
    aws_cloudwatch_log_group.application_logs.name
  ]

  query_string = <<EOF
fields @timestamp, @message
| filter @message like /SLOW_QUERY/
| parse @message "duration: * ms" as duration
| filter duration > 1000
| stats count() by bin(5m)
EOF
}

# Data source for current region
data "aws_region" "current" {}