# LatticeDB Deployment Playbooks

Quick-reference guides for common deployment scenarios.

## Table of Contents

1. [Standard Production Release](#standard-production-release)
2. [Hotfix Deployment](#hotfix-deployment)
3. [Database Migration](#database-migration)
4. [Feature Flag Rollout](#feature-flag-rollout)
5. [Disaster Recovery](#disaster-recovery)
6. [Scaling Operations](#scaling-operations)
7. [Configuration Changes](#configuration-changes)

---

## Standard Production Release

**Duration**: 2-3 hours
**Team Size**: 2 engineers
**Risk Level**: Medium

### Pre-Deployment (30 min)

```bash
# 1. Set variables
export IMAGE_URI="123456789.dkr.ecr.us-east-1.amazonaws.com/latticedb:v2.1.0"
export ENVIRONMENT="production"

# 2. Pre-flight checks
./scripts/validation/pre-flight-checks.sh -e production -i $IMAGE_URI --strict

# 3. Notify stakeholders
# Post in #deployments Slack channel
```

### Deployment (90 min)

```bash
# Start canary deployment
./aws/scripts/canary-deploy.sh \
  -e production \
  -i $IMAGE_URI \
  -c 5 \
  -s 10 \
  -t 600
```

### Monitoring (30 min)

- Open Grafana deployment dashboard
- Watch error rates and latency
- Monitor Slack alerts

### Post-Deployment (15 min)

```bash
# Validation
./scripts/validation/post-deployment-checks.sh -e production

# Update documentation
echo "## $(date +%Y-%m-%d) - v2.1.0 deployed successfully" >> docs/DEPLOYMENT_LOG.md
```

### Success Criteria

- ✓ All tasks running
- ✓ Error rate < 1%
- ✓ P95 latency < 500ms
- ✓ No alerts firing

---

## Hotfix Deployment

**Duration**: 30-45 minutes
**Team Size**: 1-2 engineers
**Risk Level**: High

### Critical Bug in Production

```bash
# 1. Build hotfix image
docker build -t latticedb:hotfix-$(date +%s) .
docker tag latticedb:hotfix-$(date +%s) $ECR_REPO:hotfix-latest
docker push $ECR_REPO:hotfix-latest

# 2. Skip pre-flight checks (if truly critical)
# Otherwise run: ./scripts/validation/pre-flight-checks.sh

# 3. Blue/green deployment (faster than canary)
./aws/scripts/blue-green-deploy.sh \
  -e production \
  -i $ECR_REPO:hotfix-latest \
  --no-approval

# 4. Immediate validation
./scripts/validation/smoke-tests.sh -u https://prod.latticedb.com

# 5. Monitor for 1 hour
# Watch dashboards closely
```

### Rollback Decision Point

If error rate > 5% after 5 minutes:
```bash
# Immediate rollback
./aws/scripts/manage-service.sh rollback -e production
```

---

## Database Migration

**Duration**: Varies (plan for 2x estimated time)
**Team Size**: 2-3 engineers
**Risk Level**: High

### Before Migration

```bash
# 1. Backup database
aws rds create-db-snapshot \
  --db-instance-identifier latticedb-prod \
  --db-snapshot-identifier latticedb-prod-pre-migration-$(date +%Y%m%d)

# 2. Test migration in staging
# Run migration script in staging environment
# Validate application works with new schema

# 3. Enable read-only mode (if needed)
./scripts/feature-flags/manage-flags.sh enable maintenance_mode
```

### Migration Process

```bash
# 1. Run migration
# (Use your migration tool, e.g., Flyway, Liquibase)
flyway migrate -url=$DB_URL -user=$DB_USER -password=$DB_PASSWORD

# 2. Deploy new application version
./aws/scripts/blue-green-deploy.sh \
  -e production \
  -i $IMAGE_WITH_MIGRATION_SUPPORT

# 3. Validate
./scripts/validation/smoke-tests.sh -u https://prod.latticedb.com -v

# 4. Disable read-only mode
./scripts/feature-flags/manage-flags.sh disable maintenance_mode
```

### Rollback Plan

```bash
# If migration fails
# 1. Restore database snapshot
aws rds restore-db-instance-from-db-snapshot \
  --db-instance-identifier latticedb-prod-restore \
  --db-snapshot-identifier latticedb-prod-pre-migration-$(date +%Y%m%d)

# 2. Rollback application
./aws/scripts/manage-service.sh rollback -e production
```

---

## Feature Flag Rollout

**Duration**: Hours to days
**Team Size**: 1 engineer
**Risk Level**: Low to Medium

### Gradual Feature Rollout

```bash
# Day 1: Internal users only
./scripts/feature-flags/manage-flags.sh enable new_query_optimizer
./scripts/feature-flags/manage-flags.sh set-percentage new_query_optimizer 0

# Manually add internal user IDs to flag targeting

# Day 2: 5% of users
./scripts/feature-flags/manage-flags.sh set-percentage new_query_optimizer 5

# Monitor for 24 hours

# Day 3: 25% of users
./scripts/feature-flags/manage-flags.sh set-percentage new_query_optimizer 25

# Monitor for 24 hours

# Day 4: 50% of users
./scripts/feature-flags/manage-flags.sh set-percentage new_query_optimizer 50

# Day 5: 100% of users
./scripts/feature-flags/manage-flags.sh set-percentage new_query_optimizer 100
```

### Monitoring

```bash
# Generate flag report
./scripts/feature-flags/manage-flags.sh report new_query_optimizer

# Check metrics in Grafana
# Compare treatment vs control groups
```

### Emergency Disable

```bash
# If issues detected
./scripts/feature-flags/manage-flags.sh disable new_query_optimizer

# Or use kill switch
redis-cli SET latticedb:flags:kill_switch:experimental true
```

---

## Disaster Recovery

**Duration**: 1-4 hours
**Team Size**: All hands on deck
**Risk Level**: Critical

### Scenario: Complete Region Failure

```bash
# 1. Verify backup region is healthy
aws ecs describe-clusters --cluster latticedb-production-us-west-2 --region us-west-2

# 2. Update DNS to point to backup region
# (Use Route53 or your DNS provider)
aws route53 change-resource-record-sets \
  --hosted-zone-id $ZONE_ID \
  --change-batch file://dns-failover.json

# 3. Scale up backup region
aws ecs update-service \
  --cluster latticedb-production-us-west-2 \
  --service latticedb-service-production \
  --desired-count 10 \
  --region us-west-2

# 4. Validate
./scripts/validation/smoke-tests.sh -u https://prod.latticedb.com
```

### Scenario: Data Corruption

```bash
# 1. Stop all writes
./scripts/feature-flags/manage-flags.sh enable read_only_mode

# 2. Identify last known good backup
aws rds describe-db-snapshots \
  --db-instance-identifier latticedb-prod \
  --query 'DBSnapshots[*].[DBSnapshotIdentifier,SnapshotCreateTime]' \
  --output table

# 3. Restore from backup
aws rds restore-db-instance-from-db-snapshot \
  --db-instance-identifier latticedb-prod-restored \
  --db-snapshot-identifier $SNAPSHOT_ID

# 4. Point application to restored database
# Update connection strings and redeploy

# 5. Validate data integrity
# Run data validation queries

# 6. Re-enable writes
./scripts/feature-flags/manage-flags.sh disable read_only_mode
```

---

## Scaling Operations

### Manual Scale Up

```bash
# Scale service
aws ecs update-service \
  --cluster latticedb-production \
  --service latticedb-service-production \
  --desired-count 10

# Verify scaling
watch -n 5 'aws ecs describe-services \
  --cluster latticedb-production \
  --service latticedb-service-production \
  --query "services[0].[runningCount,desiredCount]"'
```

### Manual Scale Down

```bash
# Scale down gradually
for count in 8 6 4 3; do
  aws ecs update-service \
    --cluster latticedb-production \
    --service latticedb-service-production \
    --desired-count $count

  echo "Scaled to $count, waiting 5 minutes..."
  sleep 300
done
```

### Auto-Scaling Configuration

```bash
# Enable auto-scaling
aws application-autoscaling register-scalable-target \
  --service-namespace ecs \
  --scalable-dimension ecs:service:DesiredCount \
  --resource-id service/latticedb-production/latticedb-service-production \
  --min-capacity 3 \
  --max-capacity 10

# Create scaling policy
aws application-autoscaling put-scaling-policy \
  --service-namespace ecs \
  --scalable-dimension ecs:service:DesiredCount \
  --resource-id service/latticedb-production/latticedb-service-production \
  --policy-name cpu-scaling-policy \
  --policy-type TargetTrackingScaling \
  --target-tracking-scaling-policy-configuration file://scaling-policy.json
```

---

## Configuration Changes

### Update Environment Variables

```bash
# 1. Get current task definition
aws ecs describe-task-definition \
  --task-definition latticedb-task-production \
  --query 'taskDefinition' > current-task-def.json

# 2. Edit environment variables
# Modify current-task-def.json

# 3. Register new task definition
aws ecs register-task-definition --cli-input-json file://current-task-def.json

# 4. Update service
NEW_TASK_DEF=$(aws ecs describe-task-definition \
  --task-definition latticedb-task-production \
  --query 'taskDefinition.taskDefinitionArn' \
  --output text)

aws ecs update-service \
  --cluster latticedb-production \
  --service latticedb-service-production \
  --task-definition $NEW_TASK_DEF
```

### Update Secrets

```bash
# 1. Update secret in Secrets Manager
aws secretsmanager update-secret \
  --secret-id latticedb/production/db-password \
  --secret-string "new-password"

# 2. Force new deployment (to pick up new secret)
aws ecs update-service \
  --cluster latticedb-production \
  --service latticedb-service-production \
  --force-new-deployment
```

### Update Rate Limits

```bash
# Edit rate limiting config
vi config/resilience/rate-limiting.yaml

# Apply changes (requires nginx reload)
# If using nginx sidecar:
kubectl rollout restart deployment/nginx-ingress

# If using ALB:
# Update listener rules via terraform or AWS console
```

---

## Common Commands Quick Reference

### Deployment

```bash
# Standard canary
./aws/scripts/canary-deploy.sh -e prod -i $IMAGE

# Blue/green
./aws/scripts/blue-green-deploy.sh -e prod -i $IMAGE

# Rollback
./aws/scripts/manage-service.sh rollback -e prod
```

### Validation

```bash
# Pre-flight
./scripts/validation/pre-flight-checks.sh -e prod -i $IMAGE

# Smoke tests
./scripts/validation/smoke-tests.sh -u https://prod.latticedb.com

# Post-deployment
./scripts/validation/post-deployment-checks.sh -e prod
```

### Feature Flags

```bash
# List flags
./scripts/feature-flags/manage-flags.sh list

# Enable flag
./scripts/feature-flags/manage-flags.sh enable FLAG_NAME

# Set percentage
./scripts/feature-flags/manage-flags.sh set-percentage FLAG_NAME 25

# Generate report
./scripts/feature-flags/manage-flags.sh report FLAG_NAME
```

### Monitoring

```bash
# Check service health
./scripts/shared/health-check.sh

# View logs
aws logs tail /ecs/latticedb-production --follow

# Check metrics
# Open Grafana: https://grafana.latticedb.com/d/deployment
```

---

## Decision Matrix

| Scenario | Strategy | Timeline | Risk |
|----------|----------|----------|------|
| Regular release | Canary | 2-3 hours | Low |
| Major version | Blue/Green | 30-60 min | Medium |
| Hotfix | Blue/Green | 30 min | High |
| Feature rollout | Feature Flag | Days | Low |
| Config change | Rolling | 15 min | Low |
| DB Migration | Blue/Green + Maintenance | 2-4 hours | High |

---

**Last Updated**: 2025-01-27
**Version**: 1.0
**Maintained By**: DevOps Team
