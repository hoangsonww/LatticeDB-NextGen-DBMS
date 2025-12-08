# LatticeDB Deployment Runbook

## Table of Contents

1. [Overview](#overview)
2. [Deployment Strategies](#deployment-strategies)
3. [Pre-Deployment Checklist](#pre-deployment-checklist)
4. [Deployment Procedures](#deployment-procedures)
5. [Rollback Procedures](#rollback-procedures)
6. [Monitoring and Validation](#monitoring-and-validation)
7. [Troubleshooting](#troubleshooting)
8. [Emergency Procedures](#emergency-procedures)
9. [Post-Deployment](#post-deployment)

---

## Overview

This runbook provides step-by-step procedures for deploying LatticeDB across different environments using various deployment strategies.

### Deployment Principles

- **Safety First**: Always validate before promoting
- **Gradual Rollout**: Prefer canary/blue-green over big-bang deployments
- **Automated Validation**: Use automated checks at every stage
- **Quick Rollback**: Be prepared to rollback at any time
- **Clear Communication**: Notify stakeholders before, during, and after

### Environments

- **Development**: Rapid iteration, minimal validation
- **Staging**: Production-like, full validation
- **Production**: Maximum safety, gradual rollout

---

## Deployment Strategies

### 1. Rolling Deployment

**When to Use**: Minor updates, bug fixes in dev/staging

**Pros**: Simple, no additional infrastructure
**Cons**: Mixed versions during deployment, harder to rollback

```bash
# Execute rolling deployment
./aws/scripts/manage-service.sh update \
  -i $IMAGE_URI \
  -e staging
```

### 2. Blue/Green Deployment

**When to Use**: Major version changes, database schema changes

**Pros**: Instant cutover, easy rollback
**Cons**: Requires double infrastructure during deployment

**Procedure**:

```bash
# 1. Run pre-flight checks
./scripts/validation/pre-flight-checks.sh \
  -e production \
  -i $IMAGE_URI

# 2. Execute blue/green deployment
./aws/scripts/blue-green-deploy.sh \
  -e production \
  -i $IMAGE_URI

# The script will:
# - Deploy green version
# - Validate health
# - Request approval
# - Switch traffic
# - Cleanup blue version
```

**Timeline**: ~15-30 minutes

### 3. Canary Deployment

**When to Use**: Production deployments with high confidence

**Pros**: Gradual rollout, early detection of issues
**Cons**: Longer deployment time

**Procedure**:

```bash
# 1. Pre-flight checks
./scripts/validation/pre-flight-checks.sh \
  -e production \
  -i $IMAGE_URI

# 2. Start canary deployment
./aws/scripts/canary-deploy.sh \
  -e production \
  -i $IMAGE_URI \
  -c 10 \
  -s 10 \
  -t 300

# Parameters:
# -c: Initial canary percentage (10%)
# -s: Traffic increment per step (10%)
# -t: Time between increments (300s)
```

**Timeline**: ~1-2 hours (depends on increment interval)

### 4. Progressive Delivery

**When to Use**: Complex deployments requiring multiple validation stages

**Procedure**:

```bash
./scripts/progressive-delivery.sh \
  -s canary \
  -e production \
  -i $IMAGE_URI \
  --notify
```

**Timeline**: Varies by strategy

---

## Pre-Deployment Checklist

### Required Information

- [ ] Image URI (fully qualified with tag)
- [ ] Target environment
- [ ] Deployment strategy
- [ ] Rollback plan
- [ ] Stakeholder notification

### Pre-Flight Checks

Run automated pre-flight checks:

```bash
./scripts/validation/pre-flight-checks.sh \
  -e production \
  -i $IMAGE_URI \
  --strict
```

Checks performed:
- ✓ Required tools available
- ✓ AWS credentials valid
- ✓ Image exists and scanned
- ✓ Current deployment healthy
- ✓ Resource capacity available
- ✓ Configuration files valid
- ✓ Deployment window appropriate
- ✓ No active incidents
- ✓ Rollback plan available

### Manual Checks

- [ ] Review release notes
- [ ] Verify database migrations (if any)
- [ ] Check for breaking API changes
- [ ] Confirm monitoring dashboards accessible
- [ ] Verify on-call schedule

---

## Deployment Procedures

### Standard Production Deployment

**Timeline**: 2-3 hours
**Strategy**: Canary
**Team**: 2 engineers minimum

#### Step 1: Preparation (15 min)

```bash
# Set environment variables
export IMAGE_URI="123456789.dkr.ecr.us-east-1.amazonaws.com/latticedb:v2.0.0"
export ENVIRONMENT="production"
export AWS_REGION="us-east-1"

# Verify image
aws ecr describe-images \
  --repository-name latticedb \
  --image-ids imageTag=v2.0.0
```

#### Step 2: Pre-Flight Validation (10 min)

```bash
./scripts/validation/pre-flight-checks.sh \
  -e production \
  -i $IMAGE_URI \
  --strict
```

**Exit Criteria**: All checks pass

#### Step 3: Deploy Canary (15 min)

```bash
./aws/scripts/canary-deploy.sh \
  -e production \
  -i $IMAGE_URI \
  -c 5 \
  -s 10 \
  -t 600
```

Monitor:
- CloudWatch metrics
- Grafana deployment dashboard
- Application logs

#### Step 4: Validation (30 min)

```bash
# Run smoke tests
./scripts/validation/smoke-tests.sh \
  -u https://prod.latticedb.com \
  -v

# Monitor metrics
# Open: https://grafana.latticedb.com/d/deployment
```

**Key Metrics**:
- Error rate < 5%
- P95 latency < 1000ms
- Success rate > 95%

#### Step 5: Progressive Traffic Shift (60-90 min)

The canary script automatically:
- Shifts 5% → 15% → 25% → 50% → 100%
- 10-minute intervals between shifts
- Validates metrics at each step
- Auto-rollback if thresholds exceeded

#### Step 6: Post-Deployment Validation (15 min)

```bash
./scripts/validation/post-deployment-checks.sh \
  -e production \
  --timeout 600
```

#### Step 7: Cleanup and Documentation (10 min)

- [ ] Update deployment log
- [ ] Notify stakeholders
- [ ] Document any issues
- [ ] Update runbook if needed

---

## Rollback Procedures

### Automatic Rollback

Both blue/green and canary scripts support automatic rollback:

- Triggered by health check failures
- Triggered by metric threshold violations
- Controlled by `--no-rollback` flag (disable if needed)

### Manual Rollback

#### Quick Rollback (Blue/Green)

If green version is still deployed but blue exists:

```bash
# Switch traffic back to blue
aws elbv2 modify-listener \
  --listener-arn $LISTENER_ARN \
  --default-actions Type=forward,TargetGroupArn=$BLUE_TG_ARN
```

#### Rollback to Previous Task Definition

```bash
# List task definitions
aws ecs list-task-definitions \
  --family-prefix latticedb-task-production \
  --sort DESC \
  --max-items 5

# Update service to previous version
aws ecs update-service \
  --cluster latticedb-production \
  --service latticedb-service-production \
  --task-definition latticedb-task-production:42
```

#### Emergency Rollback

```bash
# Use manage service script
./aws/scripts/manage-service.sh rollback \
  -e production
```

**Timeline**: 5-10 minutes

### Rollback Validation

After rollback:

```bash
# Verify service health
./scripts/shared/health-check.sh

# Check error rates
# Monitor dashboards for 15 minutes
```

---

## Monitoring and Validation

### Real-Time Monitoring

#### Grafana Dashboards

- **Main Dashboard**: https://grafana.latticedb.com/d/deployment
- **Service Health**: https://grafana.latticedb.com/d/service-health
- **Error Tracking**: https://grafana.latticedb.com/d/errors

#### Key Metrics to Watch

```
latticedb_deployment_status          # 0=failed, 1=in_progress, 2=success
latticedb_service_healthy_instances  # Number of healthy instances
latticedb_http_requests_total        # Total requests
latticedb_request_duration_seconds   # Request latency
latticedb_circuit_breaker_state      # Circuit breaker status
```

#### CloudWatch Alarms

```bash
# List active alarms
aws cloudwatch describe-alarms \
  --state-value ALARM \
  --region us-east-1
```

### Smoke Tests

```bash
# Comprehensive smoke tests
./scripts/validation/smoke-tests.sh \
  -u https://prod.latticedb.com \
  -v \
  -o /tmp/smoke-test-report.json

# View report
cat /tmp/smoke-test-report.json | jq .
```

### Log Analysis

```bash
# Recent errors from CloudWatch
aws logs filter-log-events \
  --log-group-name /ecs/latticedb-production \
  --filter-pattern "ERROR" \
  --start-time $(date -d '10 minutes ago' +%s)000
```

---

## Troubleshooting

### Common Issues

#### Issue: Tasks failing health checks

**Symptoms**:
- Tasks start but are marked unhealthy
- Tasks continuously restart

**Diagnosis**:

```bash
# Check task logs
aws ecs describe-tasks \
  --cluster latticedb-production \
  --tasks $TASK_ARN \
  --query 'tasks[0].containers[0].lastStatus'

# View logs
aws logs tail /ecs/latticedb-production --follow
```

**Resolution**:
- Check application startup time
- Verify health check endpoint
- Review resource limits (CPU/memory)
- Check database connectivity

#### Issue: High error rate after deployment

**Symptoms**:
- Error rate > 5%
- Circuit breakers opening

**Diagnosis**:

```bash
# Check error logs
aws logs filter-log-events \
  --log-group-name /ecs/latticedb-production \
  --filter-pattern "ERROR" \
  --start-time $(date -d '30 minutes ago' +%s)000 \
  | jq '.events[].message'
```

**Resolution**:
- Initiate rollback immediately
- Analyze error patterns
- Review recent code changes
- Check for API changes

#### Issue: Deployment timeout

**Symptoms**:
- Deployment exceeds expected timeline
- Tasks stuck in PENDING state

**Diagnosis**:

```bash
# Check ECS events
aws ecs describe-services \
  --cluster latticedb-production \
  --services latticedb-service-production \
  --query 'services[0].events[0:10]'
```

**Resolution**:
- Check resource availability
- Verify security groups
- Check subnet capacity
- Review ECS service limits

---

## Emergency Procedures

### Critical Production Issue

1. **Immediate Actions**:
   ```bash
   # Rollback to last known good version
   ./aws/scripts/manage-service.sh rollback -e production
   ```

2. **Activate Incident Response**:
   - Page on-call engineer
   - Create incident in PagerDuty
   - Start Slack war room: #incident-YYYYMMDD

3. **Communication**:
   - Update status page
   - Notify stakeholders
   - Document timeline

### Circuit Breaker Opened

```bash
# Check circuit breaker status
./scripts/feature-flags/manage-flags.sh show circuit_breaker

# Manually close if safe
redis-cli SET latticedb:circuit:database:state closed
```

### Database Connection Issues

```bash
# Check connection pool
curl https://prod.latticedb.com/health | jq '.connection_pool'

# Restart service if needed
aws ecs update-service \
  --cluster latticedb-production \
  --service latticedb-service-production \
  --force-new-deployment
```

### Kill Switch Activation

Disable all new features:

```bash
./scripts/feature-flags/manage-flags.sh disable new_query_optimizer
./scripts/feature-flags/manage-flags.sh disable vector_search_v2
./scripts/feature-flags/manage-flags.sh disable advanced_caching
```

Or use bulk disable:

```bash
redis-cli SET latticedb:flags:kill_switch:all true
```

---

## Post-Deployment

### Validation Period

Monitor for 24 hours:
- [ ] Hour 1: Active monitoring
- [ ] Hour 6: Review metrics
- [ ] Hour 24: Final validation

### Documentation

Update deployment log in `docs/DEPLOYMENT_LOG.md`:

```markdown
## Deployment YYYY-MM-DD

- **Version**: v2.0.0
- **Strategy**: Canary
- **Duration**: 2h 15min
- **Issues**: None
- **Deployed By**: engineer@latticedb.com
```

### Retrospective

If issues occurred:
- [ ] Schedule incident review
- [ ] Document lessons learned
- [ ] Update runbook
- [ ] Improve automation

### Cleanup

```bash
# Remove old task definitions (keep last 10)
# Remove old ECR images (keep last 20)
# Clean up CloudWatch logs (if needed)
```

---

## Contacts

### On-Call Schedule

- **Primary**: Check PagerDuty
- **Secondary**: Check PagerDuty
- **Escalation**: engineering-leads@latticedb.com

### External Resources

- **Status Page**: https://status.latticedb.com
- **Monitoring**: https://grafana.latticedb.com
- **Logs**: CloudWatch Logs
- **Metrics**: Prometheus + CloudWatch

---

## Appendix

### Useful Commands

```bash
# Check service status
aws ecs describe-services --cluster $CLUSTER --services $SERVICE

# View recent deployments
aws ecs describe-services --cluster $CLUSTER --services $SERVICE \
  --query 'services[0].deployments'

# Scale service
aws ecs update-service --cluster $CLUSTER --service $SERVICE \
  --desired-count 6

# Force new deployment
aws ecs update-service --cluster $CLUSTER --service $SERVICE \
  --force-new-deployment

# List task definitions
aws ecs list-task-definitions --family-prefix latticedb-task

# Stop a task
aws ecs stop-task --cluster $CLUSTER --task $TASK_ARN \
  --reason "Manual intervention"
```

### Deployment Timeline Reference

| Strategy | Dev | Staging | Production |
|----------|-----|---------|------------|
| Rolling  | 5 min | 10 min | N/A |
| Blue/Green | 10 min | 20 min | 30 min |
| Canary | 15 min | 30 min | 2 hours |

### SLO/SLA During Deployment

- **Availability**: > 99.9%
- **Error Rate**: < 1%
- **Latency P95**: < 500ms
- **Latency P99**: < 1000ms

---

**Last Updated**: 2025-01-27
**Version**: 1.0
**Owner**: DevOps Team
