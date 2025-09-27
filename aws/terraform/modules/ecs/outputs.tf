output "cluster_id" {
  description = "ID of the ECS cluster"
  value       = aws_ecs_cluster.main.id
}

output "cluster_arn" {
  description = "ARN of the ECS cluster"
  value       = aws_ecs_cluster.main.arn
}

output "cluster_name" {
  description = "Name of the ECS cluster"
  value       = aws_ecs_cluster.main.name
}

output "service_id" {
  description = "ID of the ECS service"
  value       = aws_ecs_service.app.id
}

output "service_arn" {
  description = "ARN of the ECS service"
  value       = aws_ecs_service.app.arn
}

output "service_name" {
  description = "Name of the ECS service"
  value       = aws_ecs_service.app.name
}

output "task_definition_arn" {
  description = "ARN of the task definition"
  value       = aws_ecs_task_definition.app.arn
}

output "task_definition_family" {
  description = "Family of the task definition"
  value       = aws_ecs_task_definition.app.family
}

output "task_definition_revision" {
  description = "Revision of the task definition"
  value       = aws_ecs_task_definition.app.revision
}

output "task_execution_role_arn" {
  description = "ARN of the task execution role"
  value       = aws_iam_role.task_execution.arn
}

output "task_execution_role_name" {
  description = "Name of the task execution role"
  value       = aws_iam_role.task_execution.name
}

output "task_role_arn" {
  description = "ARN of the task role"
  value       = aws_iam_role.task.arn
}

output "task_role_name" {
  description = "Name of the task role"
  value       = aws_iam_role.task.name
}

output "log_group_name" {
  description = "Name of the CloudWatch log group"
  value       = aws_cloudwatch_log_group.ecs.name
}

output "log_group_arn" {
  description = "ARN of the CloudWatch log group"
  value       = aws_cloudwatch_log_group.ecs.arn
}

output "autoscaling_target_resource_id" {
  description = "Resource ID of the auto scaling target"
  value       = var.enable_auto_scaling ? aws_appautoscaling_target.ecs[0].resource_id : null
}

output "autoscaling_cpu_policy_arn" {
  description = "ARN of the CPU auto scaling policy"
  value       = var.enable_auto_scaling ? aws_appautoscaling_policy.cpu[0].arn : null
}

output "autoscaling_memory_policy_arn" {
  description = "ARN of the memory auto scaling policy"
  value       = var.enable_auto_scaling ? aws_appautoscaling_policy.memory[0].arn : null
}

output "container_name" {
  description = "Name of the container"
  value       = var.container_name
}

output "container_port" {
  description = "Port exposed by the container"
  value       = var.container_port
}

output "service_security_groups" {
  description = "Security groups attached to the service"
  value       = var.security_group_ids
}

output "service_subnets" {
  description = "Subnets where the service is deployed"
  value       = var.subnet_ids
}

output "service_desired_count" {
  description = "Desired count of the service"
  value       = aws_ecs_service.app.desired_count
}

output "service_running_count" {
  description = "Number of running tasks in the service"
  value       = aws_ecs_service.app.running_count
}

output "service_pending_count" {
  description = "Number of pending tasks in the service"
  value       = aws_ecs_service.app.pending_count
}

output "capacity_providers" {
  description = "Capacity providers used by the cluster"
  value       = var.capacity_providers
}

output "container_insights_enabled" {
  description = "Whether Container Insights is enabled"
  value       = var.enable_container_insights
}

output "auto_scaling_enabled" {
  description = "Whether auto scaling is enabled"
  value       = var.enable_auto_scaling
}

output "auto_scaling_min_capacity" {
  description = "Minimum capacity for auto scaling"
  value       = var.enable_auto_scaling ? var.auto_scaling_min_capacity : null
}

output "auto_scaling_max_capacity" {
  description = "Maximum capacity for auto scaling"
  value       = var.enable_auto_scaling ? var.auto_scaling_max_capacity : null
}

output "deployment_configuration" {
  description = "Deployment configuration of the service"
  value = {
    maximum_percent         = var.deployment_maximum_percent
    minimum_healthy_percent = var.deployment_minimum_healthy_percent
    circuit_breaker_enabled = var.enable_deployment_circuit_breaker
    rollback_enabled       = var.enable_deployment_rollback
  }
}

output "load_balancer_attached" {
  description = "Whether a load balancer is attached to the service"
  value       = var.target_group_arn != null
}

output "target_group_arn" {
  description = "ARN of the target group attached to the service"
  value       = var.target_group_arn
}

output "task_cpu" {
  description = "CPU units allocated to the task"
  value       = var.task_cpu
}

output "task_memory" {
  description = "Memory (MiB) allocated to the task"
  value       = var.task_memory
}

output "health_check_configuration" {
  description = "Health check configuration for the container"
  value = {
    command      = var.health_check_command
    interval     = var.health_check_interval
    timeout      = var.health_check_timeout
    retries      = var.health_check_retries
    start_period = var.health_check_start_period
  }
}