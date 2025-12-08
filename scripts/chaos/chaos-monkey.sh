#!/bin/bash

# Chaos Monkey for LatticeDB
# Resilience testing through controlled chaos experiments

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Configuration
ENVIRONMENT=${ENVIRONMENT:-"staging"}
AWS_REGION=${AWS_REGION:-"us-east-1"}
CHAOS_DURATION=${CHAOS_DURATION:-300}
CHAOS_MODE=${CHAOS_MODE:-"safe"}
DRY_RUN=${DRY_RUN:-false}

# Chaos experiments
KILL_RANDOM_TASK=${KILL_RANDOM_TASK:-true}
NETWORK_LATENCY=${NETWORK_LATENCY:-false}
CPU_STRESS=${CPU_STRESS:-false}
MEMORY_STRESS=${MEMORY_STRESS:-false}
DISK_STRESS=${DISK_STRESS:-false}
PARTITION_NETWORK=${PARTITION_NETWORK:-false}

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Chaos Monkey - Resilience testing through controlled chaos

MODES:
    safe            - Safe experiments (task termination only)
    moderate        - Moderate chaos (includes resource stress)
    aggressive      - Aggressive chaos (includes network partitions)

OPTIONS:
    -e, --environment ENV       Environment (staging/prod) [default: staging]
    -m, --mode MODE            Chaos mode [default: safe]
    -d, --duration SECONDS     Duration of chaos [default: 300]
    --dry-run                  Preview chaos without executing
    --kill-tasks               Kill random tasks
    --network-latency          Inject network latency
    --cpu-stress               Stress CPU
    --memory-stress            Stress memory
    --disk-stress              Stress disk I/O
    --network-partition        Create network partitions
    -h, --help                 Show this help

EXAMPLES:
    # Safe mode in staging
    $0 -e staging -m safe -d 300

    # Dry run to see what would happen
    $0 -e staging --dry-run

    # Aggressive chaos in staging
    $0 -e staging -m aggressive -d 600

EOF
    exit 1
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -e|--environment)
            ENVIRONMENT="$2"
            shift 2
            ;;
        -m|--mode)
            CHAOS_MODE="$2"
            shift 2
            ;;
        -d|--duration)
            CHAOS_DURATION="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --kill-tasks)
            KILL_RANDOM_TASK=true
            shift
            ;;
        --network-latency)
            NETWORK_LATENCY=true
            shift
            ;;
        --cpu-stress)
            CPU_STRESS=true
            shift
            ;;
        --memory-stress)
            MEMORY_STRESS=true
            shift
            ;;
        --disk-stress)
            DISK_STRESS=true
            shift
            ;;
        --network-partition)
            PARTITION_NETWORK=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            ;;
    esac
done

# Validate environment
if [[ "$ENVIRONMENT" == "prod" ]]; then
    log_warning "WARNING: Running chaos experiments in PRODUCTION!"
    read -p "Are you absolutely sure? Type 'chaos' to confirm: " -r
    if [[ "$REPLY" != "chaos" ]]; then
        echo "Aborted"
        exit 1
    fi
fi

# Set chaos experiments based on mode
case $CHAOS_MODE in
    safe)
        KILL_RANDOM_TASK=true
        NETWORK_LATENCY=false
        CPU_STRESS=false
        MEMORY_STRESS=false
        DISK_STRESS=false
        PARTITION_NETWORK=false
        ;;
    moderate)
        KILL_RANDOM_TASK=true
        NETWORK_LATENCY=true
        CPU_STRESS=true
        MEMORY_STRESS=true
        DISK_STRESS=false
        PARTITION_NETWORK=false
        ;;
    aggressive)
        KILL_RANDOM_TASK=true
        NETWORK_LATENCY=true
        CPU_STRESS=true
        MEMORY_STRESS=true
        DISK_STRESS=true
        PARTITION_NETWORK=true
        ;;
    *)
        echo -e "${RED}Invalid chaos mode: $CHAOS_MODE${NC}"
        exit 1
        ;;
esac

# Logging
log() { echo -e "${BLUE}[CHAOS]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_experiment() { echo -e "${CYAN}[EXPERIMENT]${NC} $1"; }

CLUSTER_NAME="latticedb-${ENVIRONMENT}"
SERVICE_NAME="latticedb-service-${ENVIRONMENT}"

# Record experiment start
record_experiment() {
    local experiment=$1
    local status=$2

    log "Recording experiment: $experiment - $status"

    # In production, this would write to a database or S3
    cat >> /tmp/chaos-experiments.log <<EOF
{
    "timestamp": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
    "environment": "$ENVIRONMENT",
    "experiment": "$experiment",
    "status": "$status",
    "duration": $CHAOS_DURATION
}
EOF
}

# Chaos Experiment 1: Kill random ECS task
chaos_kill_random_task() {
    log_experiment "Kill Random Task"

    if [[ "$DRY_RUN" == "true" ]]; then
        log "DRY RUN: Would kill a random task"
        return 0
    fi

    # Get list of running tasks
    local tasks=$(aws ecs list-tasks \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --service-name "$SERVICE_NAME" \
        --desired-status RUNNING \
        --query 'taskArns[]' \
        --output text)

    if [[ -z "$tasks" ]]; then
        log_error "No running tasks found"
        return 1
    fi

    # Pick a random task
    local task_array=($tasks)
    local random_task=${task_array[$RANDOM % ${#task_array[@]}]}

    log "Terminating task: $random_task"

    aws ecs stop-task \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --task "$random_task" \
        --reason "Chaos Monkey: Resilience Testing" \
        > /dev/null

    log_success "Task terminated. ECS will automatically restart it."
    record_experiment "kill_random_task" "completed"
}

# Chaos Experiment 2: Inject network latency
chaos_network_latency() {
    log_experiment "Network Latency Injection"

    if [[ "$DRY_RUN" == "true" ]]; then
        log "DRY RUN: Would inject 100-500ms network latency"
        return 0
    fi

    # This requires pumba or toxiproxy running in the cluster
    # For now, we'll simulate with a service mesh configuration change

    log "Injecting 100-500ms network latency for ${CHAOS_DURATION}s"

    # In a real implementation, this would use Istio, Linkerd, or similar
    # For ECS, you might use AWS App Mesh fault injection

    log_warning "Network latency injection requires service mesh - simulated"
    record_experiment "network_latency" "simulated"

    sleep "$CHAOS_DURATION"

    log_success "Network latency experiment completed"
}

# Chaos Experiment 3: CPU stress
chaos_cpu_stress() {
    log_experiment "CPU Stress Test"

    if [[ "$DRY_RUN" == "true" ]]; then
        log "DRY RUN: Would stress CPU to 80% for ${CHAOS_DURATION}s"
        return 0
    fi

    # Get a random task
    local tasks=$(aws ecs list-tasks \
        --region "$AWS_REGION" \
        --cluster "$CLUSTER_NAME" \
        --service-name "$SERVICE_NAME" \
        --desired-status RUNNING \
        --query 'taskArns[0]' \
        --output text)

    if [[ -z "$tasks" || "$tasks" == "None" ]]; then
        log_error "No running tasks found"
        return 1
    fi

    log "Stressing CPU on task for ${CHAOS_DURATION}s"

    # This would use ECS exec to run stress-ng or similar
    # aws ecs execute-command --cluster $CLUSTER_NAME --task $task \
    #     --command "stress-ng --cpu 2 --timeout ${CHAOS_DURATION}s"

    log_warning "CPU stress requires ECS exec - simulated"
    record_experiment "cpu_stress" "simulated"

    sleep 10  # Simulate some duration
    log_success "CPU stress experiment completed"
}

# Chaos Experiment 4: Memory stress
chaos_memory_stress() {
    log_experiment "Memory Stress Test"

    if [[ "$DRY_RUN" == "true" ]]; then
        log "DRY RUN: Would stress memory to 80% for ${CHAOS_DURATION}s"
        return 0
    fi

    log "Stressing memory for ${CHAOS_DURATION}s"

    # This would use ECS exec to run stress-ng
    # aws ecs execute-command --cluster $CLUSTER_NAME --task $task \
    #     --command "stress-ng --vm 2 --vm-bytes 80% --timeout ${CHAOS_DURATION}s"

    log_warning "Memory stress requires ECS exec - simulated"
    record_experiment "memory_stress" "simulated"

    sleep 10
    log_success "Memory stress experiment completed"
}

# Chaos Experiment 5: Disk I/O stress
chaos_disk_stress() {
    log_experiment "Disk I/O Stress Test"

    if [[ "$DRY_RUN" == "true" ]]; then
        log "DRY RUN: Would stress disk I/O for ${CHAOS_DURATION}s"
        return 0
    fi

    log "Stressing disk I/O for ${CHAOS_DURATION}s"

    # This would use ECS exec to run fio or dd
    log_warning "Disk stress requires ECS exec - simulated"
    record_experiment "disk_stress" "simulated"

    sleep 10
    log_success "Disk stress experiment completed"
}

# Chaos Experiment 6: Network partition
chaos_network_partition() {
    log_experiment "Network Partition"

    if [[ "$DRY_RUN" == "true" ]]; then
        log "DRY RUN: Would create network partition for ${CHAOS_DURATION}s"
        return 0
    fi

    log "Creating network partition for ${CHAOS_DURATION}s"

    # This would modify security groups to isolate a task
    log_warning "Network partition requires security group changes - simulated"
    record_experiment "network_partition" "simulated"

    sleep 10
    log_success "Network partition experiment completed"
}

# Monitor system during chaos
monitor_system() {
    log "Monitoring system health during chaos..."

    local start_time=$(date +%s)
    local end_time=$((start_time + CHAOS_DURATION))

    while [[ $(date +%s) -lt $end_time ]]; do
        # Check service health
        local running=$(aws ecs describe-services \
            --region "$AWS_REGION" \
            --cluster "$CLUSTER_NAME" \
            --services "$SERVICE_NAME" \
            --query 'services[0].runningCount' \
            --output text 2>/dev/null || echo "0")

        local desired=$(aws ecs describe-services \
            --region "$AWS_REGION" \
            --cluster "$CLUSTER_NAME" \
            --services "$SERVICE_NAME" \
            --query 'services[0].desiredCount' \
            --output text 2>/dev/null || echo "0")

        log "Service health: $running/$desired tasks running"

        # Check if service is recovering
        if [[ "$running" -lt "$desired" ]]; then
            log_warning "Service is recovering from chaos"
        elif [[ "$running" -eq "$desired" ]]; then
            log_success "Service is healthy"
        fi

        sleep 30
    done
}

# Generate chaos report
generate_report() {
    log "Generating chaos experiment report..."

    local report_file="/tmp/chaos-report-$(date +%s).txt"

    cat > "$report_file" <<EOF
========================================
Chaos Monkey Report
========================================
Environment: $ENVIRONMENT
Mode: $CHAOS_MODE
Duration: ${CHAOS_DURATION}s
Dry Run: $DRY_RUN

Experiments Executed:
$(cat /tmp/chaos-experiments.log 2>/dev/null || echo "No experiments recorded")

========================================
EOF

    log "Report saved to: $report_file"
    cat "$report_file"
}

# Main execution
main() {
    log "========================================"
    log "Chaos Monkey Starting"
    log "========================================"
    log "Environment: $ENVIRONMENT"
    log "Mode: $CHAOS_MODE"
    log "Duration: ${CHAOS_DURATION}s"
    log "Dry Run: $DRY_RUN"
    log "========================================"

    # Clear previous experiment log
    > /tmp/chaos-experiments.log

    # Run chaos experiments
    if [[ "$KILL_RANDOM_TASK" == "true" ]]; then
        chaos_kill_random_task
        sleep 60  # Wait for recovery
    fi

    if [[ "$NETWORK_LATENCY" == "true" ]]; then
        chaos_network_latency &
        sleep 10
    fi

    if [[ "$CPU_STRESS" == "true" ]]; then
        chaos_cpu_stress &
        sleep 10
    fi

    if [[ "$MEMORY_STRESS" == "true" ]]; then
        chaos_memory_stress &
        sleep 10
    fi

    if [[ "$DISK_STRESS" == "true" ]]; then
        chaos_disk_stress &
        sleep 10
    fi

    if [[ "$PARTITION_NETWORK" == "true" ]]; then
        chaos_network_partition &
        sleep 10
    fi

    # Monitor system during chaos
    if [[ "$DRY_RUN" == "false" ]]; then
        monitor_system
    fi

    # Wait for all background jobs
    wait

    # Generate report
    generate_report

    log "========================================"
    log_success "Chaos Monkey Completed"
    log "========================================"
}

main
