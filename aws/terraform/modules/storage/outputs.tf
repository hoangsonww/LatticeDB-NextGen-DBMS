output "s3_bucket_id" {
  description = "ID of the S3 bucket"
  value       = aws_s3_bucket.main.id
}

output "s3_bucket_arn" {
  description = "ARN of the S3 bucket"
  value       = aws_s3_bucket.main.arn
}

output "s3_bucket_domain_name" {
  description = "Domain name of the S3 bucket"
  value       = aws_s3_bucket.main.bucket_domain_name
}

output "s3_bucket_regional_domain_name" {
  description = "Regional domain name of the S3 bucket"
  value       = aws_s3_bucket.main.bucket_regional_domain_name
}

output "s3_bucket_region" {
  description = "Region of the S3 bucket"
  value       = aws_s3_bucket.main.region
}

output "kms_key_id" {
  description = "ID of the KMS key used for encryption"
  value       = var.create_kms_key ? aws_kms_key.main[0].key_id : var.kms_key_id
}

output "kms_key_arn" {
  description = "ARN of the KMS key used for encryption"
  value       = var.create_kms_key ? aws_kms_key.main[0].arn : null
}

output "kms_alias_name" {
  description = "Name of the KMS key alias"
  value       = var.create_kms_key ? aws_kms_alias.main[0].name : null
}

output "efs_file_system_id" {
  description = "ID of the EFS file system"
  value       = var.enable_efs ? aws_efs_file_system.main[0].id : null
}

output "efs_file_system_arn" {
  description = "ARN of the EFS file system"
  value       = var.enable_efs ? aws_efs_file_system.main[0].arn : null
}

output "efs_dns_name" {
  description = "DNS name of the EFS file system"
  value       = var.enable_efs ? aws_efs_file_system.main[0].dns_name : null
}

output "efs_mount_target_ids" {
  description = "IDs of the EFS mount targets"
  value       = var.enable_efs ? aws_efs_mount_target.main[*].id : []
}

output "efs_mount_target_dns_names" {
  description = "DNS names of the EFS mount targets"
  value       = var.enable_efs ? aws_efs_mount_target.main[*].dns_name : []
}

output "efs_access_point_id" {
  description = "ID of the EFS access point"
  value       = var.enable_efs && var.create_efs_access_point ? aws_efs_access_point.main[0].id : null
}

output "efs_access_point_arn" {
  description = "ARN of the EFS access point"
  value       = var.enable_efs && var.create_efs_access_point ? aws_efs_access_point.main[0].arn : null
}

output "ebs_volume_ids" {
  description = "IDs of the EBS volumes"
  value       = var.enable_ebs ? aws_ebs_volume.data[*].id : []
}

output "ebs_volume_arns" {
  description = "ARNs of the EBS volumes"
  value       = var.enable_ebs ? aws_ebs_volume.data[*].arn : []
}

output "ebs_snapshot_ids" {
  description = "IDs of the EBS snapshots"
  value       = var.enable_ebs && var.create_ebs_snapshots ? aws_ebs_snapshot.data[*].id : []
}

output "ebs_snapshot_arns" {
  description = "ARNs of the EBS snapshots"
  value       = var.enable_ebs && var.create_ebs_snapshots ? aws_ebs_snapshot.data[*].arn : []
}

output "cloudwatch_log_group_name" {
  description = "Name of the CloudWatch log group"
  value       = var.enable_cloudwatch_logs ? aws_cloudwatch_log_group.storage[0].name : null
}

output "cloudwatch_log_group_arn" {
  description = "ARN of the CloudWatch log group"
  value       = var.enable_cloudwatch_logs ? aws_cloudwatch_log_group.storage[0].arn : null
}

output "s3_access_role_arn" {
  description = "ARN of the S3 access IAM role"
  value       = var.create_s3_access_role ? aws_iam_role.s3_access[0].arn : null
}

output "s3_access_role_name" {
  description = "Name of the S3 access IAM role"
  value       = var.create_s3_access_role ? aws_iam_role.s3_access[0].name : null
}

output "storage_configuration" {
  description = "Storage configuration summary"
  value = {
    s3_bucket_name      = aws_s3_bucket.main.id
    s3_versioning       = var.enable_versioning
    s3_encryption       = var.kms_key_id != null || var.create_kms_key ? "enabled" : "AES256"
    efs_enabled         = var.enable_efs
    efs_encrypted       = var.enable_efs ? var.efs_encrypted : null
    ebs_enabled         = var.enable_ebs
    ebs_volume_count    = var.enable_ebs ? var.ebs_volume_count : 0
    ebs_volume_size     = var.enable_ebs ? var.ebs_volume_size : 0
    ebs_volume_type     = var.enable_ebs ? var.ebs_volume_type : null
    ebs_encrypted       = var.enable_ebs ? var.ebs_encrypted : null
  }
}

output "backup_configuration" {
  description = "Backup configuration summary"
  value = {
    s3_lifecycle_enabled = var.enable_lifecycle_policy
    s3_versioning        = var.enable_versioning
    efs_backup_enabled   = var.enable_efs ? var.enable_efs_backup : false
    ebs_snapshots        = var.enable_ebs ? var.create_ebs_snapshots : false
  }
}

output "security_configuration" {
  description = "Security configuration summary"
  value = {
    s3_public_access_blocked = var.block_public_access
    s3_encryption_enabled    = true
    efs_encryption_enabled   = var.enable_efs ? var.efs_encrypted : null
    ebs_encryption_enabled   = var.enable_ebs ? var.ebs_encrypted : null
    kms_key_managed         = var.create_kms_key
  }
}

output "performance_configuration" {
  description = "Performance configuration summary"
  value = {
    efs_performance_mode      = var.enable_efs ? var.efs_performance_mode : null
    efs_throughput_mode       = var.enable_efs ? var.efs_throughput_mode : null
    efs_provisioned_throughput = var.enable_efs && var.efs_throughput_mode == "provisioned" ? var.efs_provisioned_throughput : null
    ebs_volume_type           = var.enable_ebs ? var.ebs_volume_type : null
    ebs_iops                  = var.enable_ebs && (var.ebs_volume_type == "gp3" || var.ebs_volume_type == "io1" || var.ebs_volume_type == "io2") ? var.ebs_iops : null
    ebs_throughput            = var.enable_ebs && var.ebs_volume_type == "gp3" ? var.ebs_throughput : null
  }
}

output "monitoring_configuration" {
  description = "Monitoring configuration summary"
  value = {
    cloudwatch_logs_enabled = var.enable_cloudwatch_logs
    s3_notifications_enabled = var.enable_s3_notifications
    log_retention_days      = var.enable_cloudwatch_logs ? var.log_retention_days : null
  }
}