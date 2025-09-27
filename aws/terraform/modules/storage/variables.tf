variable "bucket_name" {
  description = "Name of the S3 bucket"
  type        = string
}

variable "enable_versioning" {
  description = "Enable versioning for the S3 bucket"
  type        = bool
  default     = true
}

variable "block_public_access" {
  description = "Block public access to the S3 bucket"
  type        = bool
  default     = true
}

variable "kms_key_id" {
  description = "KMS key ID for S3 bucket encryption (optional)"
  type        = string
  default     = null
}

variable "create_kms_key" {
  description = "Create a new KMS key for encryption"
  type        = bool
  default     = false
}

variable "kms_deletion_window" {
  description = "KMS key deletion window in days"
  type        = number
  default     = 30

  validation {
    condition     = var.kms_deletion_window >= 7 && var.kms_deletion_window <= 30
    error_message = "KMS deletion window must be between 7 and 30 days."
  }
}

variable "enable_lifecycle_policy" {
  description = "Enable lifecycle policy for the S3 bucket"
  type        = bool
  default     = true
}

variable "transition_to_ia_days" {
  description = "Number of days after which objects transition to IA"
  type        = number
  default     = 30
}

variable "transition_to_glacier_days" {
  description = "Number of days after which objects transition to Glacier"
  type        = number
  default     = 90
}

variable "expire_noncurrent_versions_days" {
  description = "Number of days after which noncurrent versions expire"
  type        = number
  default     = 365
}

variable "enable_abort_incomplete_multipart_upload" {
  description = "Enable rule to abort incomplete multipart uploads"
  type        = bool
  default     = true
}

variable "enable_efs" {
  description = "Enable EFS file system"
  type        = bool
  default     = false
}

variable "efs_name" {
  description = "Name for the EFS file system"
  type        = string
  default     = "latticedb-efs"
}

variable "efs_creation_token" {
  description = "Creation token for the EFS file system"
  type        = string
  default     = null
}

variable "efs_performance_mode" {
  description = "Performance mode for EFS"
  type        = string
  default     = "generalPurpose"

  validation {
    condition     = contains(["generalPurpose", "maxIO"], var.efs_performance_mode)
    error_message = "EFS performance mode must be generalPurpose or maxIO."
  }
}

variable "efs_throughput_mode" {
  description = "Throughput mode for EFS"
  type        = string
  default     = "bursting"

  validation {
    condition     = contains(["bursting", "provisioned"], var.efs_throughput_mode)
    error_message = "EFS throughput mode must be bursting or provisioned."
  }
}

variable "efs_provisioned_throughput" {
  description = "Provisioned throughput in MiB/s (only for provisioned mode)"
  type        = number
  default     = null
}

variable "efs_encrypted" {
  description = "Enable encryption for EFS"
  type        = bool
  default     = true
}

variable "efs_kms_key_id" {
  description = "KMS key ID for EFS encryption"
  type        = string
  default     = null
}

variable "enable_efs_backup" {
  description = "Enable automatic backups for EFS"
  type        = bool
  default     = true
}

variable "efs_subnet_ids" {
  description = "List of subnet IDs for EFS mount targets"
  type        = list(string)
  default     = []
}

variable "efs_security_group_ids" {
  description = "List of security group IDs for EFS mount targets"
  type        = list(string)
  default     = []
}

variable "create_efs_access_point" {
  description = "Create an EFS access point"
  type        = bool
  default     = false
}

variable "efs_access_point_path" {
  description = "Path for the EFS access point"
  type        = string
  default     = "/latticedb"
}

variable "efs_access_point_uid" {
  description = "UID for the EFS access point"
  type        = number
  default     = 1001
}

variable "efs_access_point_gid" {
  description = "GID for the EFS access point"
  type        = number
  default     = 1001
}

variable "efs_access_point_permissions" {
  description = "Permissions for the EFS access point"
  type        = string
  default     = "755"
}

variable "enable_ebs" {
  description = "Enable EBS volumes"
  type        = bool
  default     = false
}

variable "ebs_volume_count" {
  description = "Number of EBS volumes to create"
  type        = number
  default     = 1
}

variable "ebs_availability_zone" {
  description = "Availability zone for EBS volumes"
  type        = string
  default     = null
}

variable "ebs_volume_size" {
  description = "Size of EBS volumes in GB"
  type        = number
  default     = 100
}

variable "ebs_volume_type" {
  description = "Type of EBS volumes"
  type        = string
  default     = "gp3"

  validation {
    condition     = contains(["gp2", "gp3", "io1", "io2", "sc1", "st1"], var.ebs_volume_type)
    error_message = "EBS volume type must be one of: gp2, gp3, io1, io2, sc1, st1."
  }
}

variable "ebs_iops" {
  description = "IOPS for EBS volumes (gp3, io1, io2)"
  type        = number
  default     = null
}

variable "ebs_throughput" {
  description = "Throughput for EBS volumes (gp3 only)"
  type        = number
  default     = null
}

variable "ebs_encrypted" {
  description = "Enable encryption for EBS volumes"
  type        = bool
  default     = true
}

variable "ebs_kms_key_id" {
  description = "KMS key ID for EBS encryption"
  type        = string
  default     = null
}

variable "ebs_name_prefix" {
  description = "Name prefix for EBS volumes"
  type        = string
  default     = "latticedb-data"
}

variable "create_ebs_snapshots" {
  description = "Create initial snapshots of EBS volumes"
  type        = bool
  default     = false
}

variable "enable_cloudwatch_logs" {
  description = "Enable CloudWatch logs for storage operations"
  type        = bool
  default     = true
}

variable "log_retention_days" {
  description = "Number of days to retain CloudWatch logs"
  type        = number
  default     = 14

  validation {
    condition = contains([1, 3, 5, 7, 14, 30, 60, 90, 120, 150, 180, 365, 400, 545, 731, 1827, 3653], var.log_retention_days)
    error_message = "Log retention must be one of the allowed CloudWatch values."
  }
}

variable "enable_s3_notifications" {
  description = "Enable S3 bucket notifications"
  type        = bool
  default     = false
}

variable "s3_cloudwatch_configurations" {
  description = "S3 CloudWatch notification configurations"
  type = list(object({
    events        = list(string)
    filter_prefix = string
    filter_suffix = string
  }))
  default = []
}

variable "s3_lambda_configurations" {
  description = "S3 Lambda notification configurations"
  type = list(object({
    lambda_function_arn = string
    events             = list(string)
    filter_prefix      = string
    filter_suffix      = string
  }))
  default = []
}

variable "s3_sns_configurations" {
  description = "S3 SNS notification configurations"
  type = list(object({
    topic_arn     = string
    events        = list(string)
    filter_prefix = string
    filter_suffix = string
  }))
  default = []
}

variable "create_s3_access_role" {
  description = "Create an IAM role for S3 access"
  type        = bool
  default     = false
}

variable "s3_access_role_principals" {
  description = "List of principals that can assume the S3 access role"
  type        = list(string)
  default     = ["ec2.amazonaws.com", "ecs-tasks.amazonaws.com"]
}

variable "tags" {
  description = "Tags to apply to all resources"
  type        = map(string)
  default     = {}
}