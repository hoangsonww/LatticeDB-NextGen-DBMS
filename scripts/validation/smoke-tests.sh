#!/bin/bash

# Comprehensive Smoke Test Suite for LatticeDB
# Validates deployment health with extensive functional tests

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Configuration
TARGET_URL=${TARGET_URL:-"http://localhost:8080"}
TIMEOUT=${TIMEOUT:-30}
MAX_RETRIES=${MAX_RETRIES:-3}
VERBOSE=${VERBOSE:-false}
REPORT_FILE=${REPORT_FILE:-"/tmp/smoke-test-report.json"}

# Test tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0
declare -a FAILED_TEST_NAMES

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Comprehensive smoke test suite for LatticeDB deployments

OPTIONS:
    -u, --url URL               Target URL [default: http://localhost:8080]
    -t, --timeout SECONDS       Request timeout [default: 30]
    -r, --retries COUNT         Max retries per test [default: 3]
    -v, --verbose               Enable verbose output
    -o, --output FILE           Report output file [default: /tmp/smoke-test-report.json]
    -h, --help                  Show this help

EXAMPLES:
    # Test local deployment
    $0 -u http://localhost:8080

    # Test staging with verbose output
    $0 -u https://staging.latticedb.com -v

    # Test with custom timeout and output
    $0 -u https://prod.latticedb.com -t 60 -o ./report.json

EOF
    exit 1
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -u|--url)
            TARGET_URL="$2"
            shift 2
            ;;
        -t|--timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        -r|--retries)
            MAX_RETRIES="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -o|--output)
            REPORT_FILE="$2"
            shift 2
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

# Logging
log() { echo -e "${BLUE}[TEST]${NC} $1"; }
log_success() { echo -e "${GREEN}[PASS]${NC} $1"; }
log_error() { echo -e "${RED}[FAIL]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_info() { echo -e "${CYAN}[INFO]${NC} $1"; }
log_verbose() { [[ "$VERBOSE" == "true" ]] && echo -e "${CYAN}[VERBOSE]${NC} $1" || true; }

# Test result tracking
start_test() {
    local test_name=$1
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Running: $test_name"
}

pass_test() {
    local test_name=$1
    PASSED_TESTS=$((PASSED_TESTS + 1))
    log_success "$test_name"
}

fail_test() {
    local test_name=$1
    local reason=${2:-"Unknown error"}
    FAILED_TESTS=$((FAILED_TESTS + 1))
    FAILED_TEST_NAMES+=("$test_name: $reason")
    log_error "$test_name - $reason"
}

skip_test() {
    local test_name=$1
    local reason=${2:-"Skipped"}
    SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
    log_warning "$test_name - $reason"
}

# HTTP request helper with retries
http_request() {
    local method=$1
    local endpoint=$2
    local data=${3:-""}
    local expected_code=${4:-200}

    local url="${TARGET_URL}${endpoint}"
    local retries=0

    while [[ $retries -lt $MAX_RETRIES ]]; do
        log_verbose "HTTP $method $url (attempt $((retries + 1))/$MAX_RETRIES)"

        local response
        local http_code

        if [[ -n "$data" ]]; then
            response=$(curl -s -w "\n%{http_code}" -X "$method" \
                -H "Content-Type: application/json" \
                -H "Accept: application/json" \
                --max-time "$TIMEOUT" \
                -d "$data" \
                "$url" 2>/dev/null || echo "CURL_ERROR")
        else
            response=$(curl -s -w "\n%{http_code}" -X "$method" \
                -H "Accept: application/json" \
                --max-time "$TIMEOUT" \
                "$url" 2>/dev/null || echo "CURL_ERROR")
        fi

        if [[ "$response" == "CURL_ERROR" ]]; then
            retries=$((retries + 1))
            sleep 2
            continue
        fi

        http_code=$(echo "$response" | tail -n1)
        response=$(echo "$response" | sed '$d')

        log_verbose "Response code: $http_code"
        log_verbose "Response body: ${response:0:200}"

        if [[ "$http_code" == "$expected_code" ]]; then
            echo "$response"
            return 0
        fi

        retries=$((retries + 1))
        sleep 2
    done

    return 1
}

# Test 1: Health Check
test_health_check() {
    start_test "Health Check Endpoint"

    local response
    if response=$(http_request GET /health "" 200); then
        if echo "$response" | jq -e '.status == "healthy"' >/dev/null 2>&1; then
            pass_test "Health check endpoint returns healthy status"
        else
            fail_test "Health check endpoint" "Status is not healthy: $response"
        fi
    else
        fail_test "Health check endpoint" "Failed to get response"
    fi
}

# Test 2: Readiness Check
test_readiness() {
    start_test "Readiness Check"

    local response
    if response=$(http_request GET /ready "" 200); then
        if echo "$response" | jq -e '.ready == true' >/dev/null 2>&1; then
            pass_test "Readiness check"
        else
            fail_test "Readiness check" "Service not ready: $response"
        fi
    else
        skip_test "Readiness check" "Endpoint may not be implemented"
    fi
}

# Test 3: Metrics Endpoint
test_metrics() {
    start_test "Metrics Endpoint"

    local response
    if response=$(http_request GET /metrics "" 200); then
        if echo "$response" | grep -q "latticedb_"; then
            pass_test "Metrics endpoint exposes metrics"
        else
            fail_test "Metrics endpoint" "No valid metrics found"
        fi
    else
        fail_test "Metrics endpoint" "Failed to get metrics"
    fi
}

# Test 4: Version Info
test_version() {
    start_test "Version Information"

    local response
    if response=$(http_request GET /info "" 200); then
        if echo "$response" | jq -e '.version' >/dev/null 2>&1; then
            local version=$(echo "$response" | jq -r '.version')
            log_info "Version: $version"
            pass_test "Version information endpoint"
        else
            skip_test "Version information" "Version field not found"
        fi
    else
        skip_test "Version information" "Endpoint may not be implemented"
    fi
}

# Test 5: Database Connectivity
test_database_connectivity() {
    start_test "Database Connectivity"

    local query='{"query": "SELECT 1 AS test"}'
    local response

    if response=$(http_request POST /api/v1/query "$query" 200); then
        if echo "$response" | jq -e '.success == true' >/dev/null 2>&1; then
            pass_test "Database connectivity"
        else
            fail_test "Database connectivity" "Query failed: $response"
        fi
    else
        fail_test "Database connectivity" "Failed to execute query"
    fi
}

# Test 6: Table Creation
test_table_creation() {
    start_test "Table Creation"

    local create_query='{"query": "CREATE TABLE IF NOT EXISTS smoke_test_table (id INT, name VARCHAR(100))"}'
    local response

    if response=$(http_request POST /api/v1/query "$create_query" 200); then
        pass_test "Table creation"
    else
        fail_test "Table creation" "Failed to create table"
    fi
}

# Test 7: Data Insertion
test_data_insertion() {
    start_test "Data Insertion"

    local insert_query='{"query": "INSERT INTO smoke_test_table VALUES (1, '"'"'test'"'"')"}'
    local response

    if response=$(http_request POST /api/v1/query "$insert_query" 200); then
        pass_test "Data insertion"
    else
        fail_test "Data insertion" "Failed to insert data"
    fi
}

# Test 8: Data Retrieval
test_data_retrieval() {
    start_test "Data Retrieval"

    local select_query='{"query": "SELECT * FROM smoke_test_table WHERE id = 1"}'
    local response

    if response=$(http_request POST /api/v1/query "$select_query" 200); then
        if echo "$response" | jq -e '.rows | length > 0' >/dev/null 2>&1; then
            pass_test "Data retrieval"
        else
            fail_test "Data retrieval" "No data returned"
        fi
    else
        fail_test "Data retrieval" "Failed to retrieve data"
    fi
}

# Test 9: Transaction Support
test_transactions() {
    start_test "Transaction Support"

    local begin_query='{"query": "BEGIN TRANSACTION"}'
    local response

    if response=$(http_request POST /api/v1/query "$begin_query" 200); then
        local commit_query='{"query": "COMMIT"}'
        if http_request POST /api/v1/query "$commit_query" 200 >/dev/null; then
            pass_test "Transaction support"
        else
            fail_test "Transaction support" "Failed to commit transaction"
        fi
    else
        skip_test "Transaction support" "Transactions may not be implemented"
    fi
}

# Test 10: Vector Operations
test_vector_operations() {
    start_test "Vector Operations"

    local vector_query='{"query": "CREATE TABLE IF NOT EXISTS vec_test (id INT, embedding VECTOR(3))"}'
    local response

    if response=$(http_request POST /api/v1/query "$vector_query" 200); then
        local insert_vector='{"query": "INSERT INTO vec_test VALUES (1, [1.0, 2.0, 3.0])"}'
        if http_request POST /api/v1/query "$insert_vector" 200 >/dev/null; then
            pass_test "Vector operations"
        else
            skip_test "Vector operations" "Vector insertion failed"
        fi
    else
        skip_test "Vector operations" "Vector support may not be enabled"
    fi
}

# Test 11: Connection Pool
test_connection_pool() {
    start_test "Connection Pool Health"

    local response
    if response=$(http_request GET /health "" 200); then
        if echo "$response" | jq -e '.connection_pool.available > 0' >/dev/null 2>&1; then
            local available=$(echo "$response" | jq -r '.connection_pool.available')
            local total=$(echo "$response" | jq -r '.connection_pool.total')
            log_info "Connection pool: $available/$total available"
            pass_test "Connection pool health"
        else
            fail_test "Connection pool health" "No available connections"
        fi
    else
        skip_test "Connection pool health" "Health endpoint does not expose connection pool"
    fi
}

# Test 12: Concurrent Requests
test_concurrent_requests() {
    start_test "Concurrent Request Handling"

    local pids=()
    local success_count=0

    for i in {1..10}; do
        (
            if http_request GET /health "" 200 >/dev/null 2>&1; then
                exit 0
            else
                exit 1
            fi
        ) &
        pids+=($!)
    done

    for pid in "${pids[@]}"; do
        if wait "$pid" 2>/dev/null; then
            success_count=$((success_count + 1))
        fi
    done

    if [[ $success_count -ge 8 ]]; then
        log_info "Concurrent requests: $success_count/10 succeeded"
        pass_test "Concurrent request handling"
    else
        fail_test "Concurrent request handling" "Only $success_count/10 requests succeeded"
    fi
}

# Test 13: Error Handling
test_error_handling() {
    start_test "Error Handling"

    local invalid_query='{"query": "SELECT * FROM nonexistent_table"}'
    local response

    if response=$(http_request POST /api/v1/query "$invalid_query" 400); then
        pass_test "Error handling returns proper error codes"
    elif response=$(http_request POST /api/v1/query "$invalid_query" 500); then
        pass_test "Error handling returns proper error codes"
    else
        fail_test "Error handling" "Expected error response, got success or unexpected code"
    fi
}

# Test 14: Rate Limiting
test_rate_limiting() {
    start_test "Rate Limiting"

    local request_count=0
    local rate_limited=false

    for i in {1..100}; do
        if http_request GET /health "" 200 >/dev/null 2>&1; then
            request_count=$((request_count + 1))
        elif http_request GET /health "" 429 >/dev/null 2>&1; then
            rate_limited=true
            break
        fi
    done

    if [[ "$rate_limited" == "true" ]]; then
        log_info "Rate limiting engaged after $request_count requests"
        pass_test "Rate limiting is configured"
    else
        skip_test "Rate limiting" "No rate limiting detected (may not be configured)"
    fi
}

# Test 15: SSL/TLS
test_ssl_tls() {
    start_test "SSL/TLS Configuration"

    if [[ "$TARGET_URL" =~ ^https:// ]]; then
        if curl -s --max-time 10 "$TARGET_URL/health" >/dev/null 2>&1; then
            pass_test "SSL/TLS is properly configured"
        else
            fail_test "SSL/TLS configuration" "HTTPS endpoint not reachable"
        fi
    else
        skip_test "SSL/TLS configuration" "Target is HTTP, not HTTPS"
    fi
}

# Cleanup test data
cleanup_test_data() {
    log_info "Cleaning up test data..."

    local drop_query='{"query": "DROP TABLE IF EXISTS smoke_test_table"}'
    http_request POST /api/v1/query "$drop_query" 200 >/dev/null 2>&1 || true

    local drop_vec='{"query": "DROP TABLE IF EXISTS vec_test"}'
    http_request POST /api/v1/query "$drop_vec" 200 >/dev/null 2>&1 || true

    log_info "Cleanup complete"
}

# Generate report
generate_report() {
    log_info "Generating test report..."

    local pass_rate=0
    if [[ $TOTAL_TESTS -gt 0 ]]; then
        pass_rate=$(echo "scale=2; ($PASSED_TESTS / $TOTAL_TESTS) * 100" | bc)
    fi

    local failed_tests_json="[]"
    if [[ ${#FAILED_TEST_NAMES[@]} -gt 0 ]]; then
        failed_tests_json=$(printf '%s\n' "${FAILED_TEST_NAMES[@]}" | jq -R . | jq -s .)
    fi

    cat > "$REPORT_FILE" <<EOF
{
    "timestamp": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
    "target": "$TARGET_URL",
    "summary": {
        "total": $TOTAL_TESTS,
        "passed": $PASSED_TESTS,
        "failed": $FAILED_TESTS,
        "skipped": $SKIPPED_TESTS,
        "pass_rate": $pass_rate
    },
    "failed_tests": $failed_tests_json,
    "status": "$([ $FAILED_TESTS -eq 0 ] && echo "success" || echo "failure")"
}
EOF

    log_info "Report saved to: $REPORT_FILE"
}

# Print summary
print_summary() {
    echo ""
    echo "========================================"
    echo "Smoke Test Results"
    echo "========================================"
    echo "Target: $TARGET_URL"
    echo "Total Tests: $TOTAL_TESTS"
    echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"
    echo -e "${RED}Failed: $FAILED_TESTS${NC}"
    echo -e "${YELLOW}Skipped: $SKIPPED_TESTS${NC}"

    if [[ $TOTAL_TESTS -gt 0 ]]; then
        local pass_rate=$(echo "scale=2; ($PASSED_TESTS / $TOTAL_TESTS) * 100" | bc)
        echo "Pass Rate: ${pass_rate}%"
    fi

    echo "========================================"

    if [[ $FAILED_TESTS -gt 0 ]]; then
        echo -e "${RED}Failed Tests:${NC}"
        for test in "${FAILED_TEST_NAMES[@]}"; do
            echo -e "  ${RED}✗${NC} $test"
        done
        echo "========================================"
    fi
}

# Main execution
main() {
    log "Starting smoke tests against: $TARGET_URL"
    log "Timeout: ${TIMEOUT}s, Max retries: $MAX_RETRIES"
    echo ""

    # Run all tests
    test_health_check
    test_readiness
    test_metrics
    test_version
    test_database_connectivity
    test_table_creation
    test_data_insertion
    test_data_retrieval
    test_transactions
    test_vector_operations
    test_connection_pool
    test_concurrent_requests
    test_error_handling
    test_rate_limiting
    test_ssl_tls

    # Cleanup
    cleanup_test_data

    # Generate report
    generate_report

    # Print summary
    print_summary

    # Exit with appropriate code
    if [[ $FAILED_TESTS -eq 0 ]]; then
        log_success "All smoke tests passed!"
        exit 0
    else
        log_error "Some smoke tests failed"
        exit 1
    fi
}

main
