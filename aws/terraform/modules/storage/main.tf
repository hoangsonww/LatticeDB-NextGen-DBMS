terraform {
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

# S3 Bucket for data storage and backups
resource "aws_s3_bucket" "main" {
  bucket = var.bucket_name

  tags = var.tags
}

resource "aws_s3_bucket_versioning" "main" {
  bucket = aws_s3_bucket.main.id

  versioning_configuration {
    status = var.enable_versioning ? "Enabled" : "Suspended"
  }
}

resource "aws_s3_bucket_server_side_encryption_configuration" "main" {
  bucket = aws_s3_bucket.main.id

  rule {
    apply_server_side_encryption_by_default {
      kms_master_key_id = var.kms_key_id
      sse_algorithm     = var.kms_key_id != null ? "aws:kms" : "AES256"
    }
    bucket_key_enabled = var.kms_key_id != null
  }
}

resource "aws_s3_bucket_public_access_block" "main" {
  bucket = aws_s3_bucket.main.id

  block_public_acls       = var.block_public_access
  block_public_policy     = var.block_public_access
  ignore_public_acls      = var.block_public_access
  restrict_public_buckets = var.block_public_access
}

resource "aws_s3_bucket_lifecycle_configuration" "main" {
  count = var.enable_lifecycle_policy ? 1 : 0

  bucket = aws_s3_bucket.main.id

  rule {
    id     = "transition_to_ia"
    status = "Enabled"

    transition {
      days          = var.transition_to_ia_days
      storage_class = "STANDARD_IA"
    }
  }

  rule {
    id     = "transition_to_glacier"
    status = "Enabled"

    transition {
      days          = var.transition_to_glacier_days
      storage_class = "GLACIER"
    }
  }

  rule {
    id     = "expire_old_versions"
    status = "Enabled"

    noncurrent_version_expiration {
      noncurrent_days = var.expire_noncurrent_versions_days
    }
  }

  dynamic "rule" {
    for_each = var.enable_abort_incomplete_multipart_upload ? [1] : []
    content {
      id     = "abort_incomplete_multipart_upload"
      status = "Enabled"

      abort_incomplete_multipart_upload {
        days_after_initiation = 7
      }
    }
  }
}

# EFS File System for persistent shared storage
resource "aws_efs_file_system" "main" {
  count = var.enable_efs ? 1 : 0

  creation_token   = var.efs_creation_token
  performance_mode = var.efs_performance_mode
  throughput_mode  = var.efs_throughput_mode

  dynamic "provisioned_throughput_in_mibps" {
    for_each = var.efs_throughput_mode == "provisioned" ? [var.efs_provisioned_throughput] : []
    content {
      provisioned_throughput_in_mibps = provisioned_throughput_in_mibps.value
    }
  }

  encrypted  = var.efs_encrypted
  kms_key_id = var.efs_encrypted ? var.efs_kms_key_id : null

  tags = merge(var.tags, {
    Name = var.efs_name
  })
}

resource "aws_efs_backup_policy" "main" {
  count = var.enable_efs && var.enable_efs_backup ? 1 : 0

  file_system_id = aws_efs_file_system.main[0].id

  backup_policy {
    status = "ENABLED"
  }
}

# EFS Mount Targets
resource "aws_efs_mount_target" "main" {
  count = var.enable_efs ? length(var.efs_subnet_ids) : 0

  file_system_id  = aws_efs_file_system.main[0].id
  subnet_id       = var.efs_subnet_ids[count.index]
  security_groups = var.efs_security_group_ids
}

# EFS Access Point
resource "aws_efs_access_point" "main" {
  count = var.enable_efs && var.create_efs_access_point ? 1 : 0

  file_system_id = aws_efs_file_system.main[0].id

  posix_user {
    gid = var.efs_access_point_gid
    uid = var.efs_access_point_uid
  }

  root_directory {
    path = var.efs_access_point_path

    creation_info {
      owner_gid   = var.efs_access_point_gid
      owner_uid   = var.efs_access_point_uid
      permissions = var.efs_access_point_permissions
    }
  }

  tags = merge(var.tags, {
    Name = "${var.efs_name}-access-point"
  })
}

# EBS Volumes for persistent database storage
resource "aws_ebs_volume" "data" {
  count = var.enable_ebs ? var.ebs_volume_count : 0

  availability_zone = var.ebs_availability_zone
  size              = var.ebs_volume_size
  type              = var.ebs_volume_type
  iops              = var.ebs_volume_type == "gp3" || var.ebs_volume_type == "io1" || var.ebs_volume_type == "io2" ? var.ebs_iops : null
  throughput        = var.ebs_volume_type == "gp3" ? var.ebs_throughput : null
  encrypted         = var.ebs_encrypted
  kms_key_id        = var.ebs_encrypted ? var.ebs_kms_key_id : null

  tags = merge(var.tags, {
    Name = "${var.ebs_name_prefix}-${count.index + 1}"
  })
}

# EBS Snapshots for backups
resource "aws_ebs_snapshot" "data" {
  count = var.enable_ebs && var.create_ebs_snapshots ? var.ebs_volume_count : 0

  volume_id   = aws_ebs_volume.data[count.index].id
  description = "Snapshot of ${var.ebs_name_prefix}-${count.index + 1}"

  tags = merge(var.tags, {
    Name = "${var.ebs_name_prefix}-snapshot-${count.index + 1}"
  })
}

# KMS Key for encryption
resource "aws_kms_key" "main" {
  count = var.create_kms_key ? 1 : 0

  description         = "KMS key for ${var.bucket_name} encryption"
  key_usage           = "ENCRYPT_DECRYPT"
  key_spec            = "SYMMETRIC_DEFAULT"
  deletion_window_in_days = var.kms_deletion_window

  tags = var.tags
}

resource "aws_kms_alias" "main" {
  count = var.create_kms_key ? 1 : 0

  name          = "alias/${var.bucket_name}-key"
  target_key_id = aws_kms_key.main[0].key_id
}

# CloudWatch Log Group for storage-related logs
resource "aws_cloudwatch_log_group" "storage" {
  count = var.enable_cloudwatch_logs ? 1 : 0

  name              = "/aws/storage/${var.bucket_name}"
  retention_in_days = var.log_retention_days

  tags = var.tags
}

# S3 Bucket Notification for CloudWatch Events
resource "aws_s3_bucket_notification" "main" {
  count = var.enable_s3_notifications ? 1 : 0

  bucket = aws_s3_bucket.main.id

  dynamic "cloudwatch_configuration" {
    for_each = var.s3_cloudwatch_configurations
    content {
      events = cloudwatch_configuration.value.events
      filter_prefix = cloudwatch_configuration.value.filter_prefix
      filter_suffix = cloudwatch_configuration.value.filter_suffix
    }
  }

  dynamic "lambda_function" {
    for_each = var.s3_lambda_configurations
    content {
      lambda_function_arn = lambda_function.value.lambda_function_arn
      events             = lambda_function.value.events
      filter_prefix      = lambda_function.value.filter_prefix
      filter_suffix      = lambda_function.value.filter_suffix
    }
  }

  dynamic "topic" {
    for_each = var.s3_sns_configurations
    content {
      topic_arn     = topic.value.topic_arn
      events        = topic.value.events
      filter_prefix = topic.value.filter_prefix
      filter_suffix = topic.value.filter_suffix
    }
  }

  depends_on = [
    aws_lambda_permission.s3_invoke,
    aws_sns_topic_policy.s3_publish
  ]
}

# Lambda permission for S3 to invoke
resource "aws_lambda_permission" "s3_invoke" {
  count = var.enable_s3_notifications && length(var.s3_lambda_configurations) > 0 ? length(var.s3_lambda_configurations) : 0

  statement_id  = "AllowExecutionFromS3Bucket"
  action        = "lambda:InvokeFunction"
  function_name = var.s3_lambda_configurations[count.index].lambda_function_arn
  principal     = "s3.amazonaws.com"
  source_arn    = aws_s3_bucket.main.arn
}

# SNS topic policy for S3 to publish
resource "aws_sns_topic_policy" "s3_publish" {
  count = var.enable_s3_notifications && length(var.s3_sns_configurations) > 0 ? length(var.s3_sns_configurations) : 0

  arn = var.s3_sns_configurations[count.index].topic_arn

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Principal = {
          Service = "s3.amazonaws.com"
        }
        Action   = "sns:Publish"
        Resource = var.s3_sns_configurations[count.index].topic_arn
        Condition = {
          ArnEquals = {
            "aws:SourceArn" = aws_s3_bucket.main.arn
          }
        }
      }
    ]
  })
}

# IAM role for S3 access
resource "aws_iam_role" "s3_access" {
  count = var.create_s3_access_role ? 1 : 0

  name = "${var.bucket_name}-s3-access-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Principal = {
          Service = var.s3_access_role_principals
        }
      }
    ]
  })

  tags = var.tags
}

resource "aws_iam_role_policy" "s3_access" {
  count = var.create_s3_access_role ? 1 : 0

  name = "${var.bucket_name}-s3-access-policy"
  role = aws_iam_role.s3_access[0].id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = concat(
      [
        {
          Effect = "Allow"
          Action = [
            "s3:GetObject",
            "s3:PutObject",
            "s3:DeleteObject",
            "s3:ListBucket"
          ]
          Resource = [
            aws_s3_bucket.main.arn,
            "${aws_s3_bucket.main.arn}/*"
          ]
        }
      ],
      var.kms_key_id != null || var.create_kms_key ? [
        {
          Effect = "Allow"
          Action = [
            "kms:Encrypt",
            "kms:Decrypt",
            "kms:ReEncrypt*",
            "kms:GenerateDataKey*",
            "kms:DescribeKey"
          ]
          Resource = var.create_kms_key ? aws_kms_key.main[0].arn : var.kms_key_id
        }
      ] : []
    )
  })
}

# Data source for current region and account
data "aws_region" "current" {}
data "aws_caller_identity" "current" {}