#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <atomic>

#ifdef HAVE_TELEMETRY
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/metrics/async_instruments.h"
#endif

namespace latticedb {
namespace telemetry {

class MetricsCollector {
public:
    static MetricsCollector& getInstance();

    void initialize();

    // Database operation metrics
    void recordQueryLatency(const std::string& query_type, double latency_ms);
    void incrementQueryCount(const std::string& query_type);
    void incrementErrorCount(const std::string& error_type);
    void recordTransactionLatency(double latency_ms);
    void incrementTransactionCount(const std::string& status);

    // System metrics
    void recordMemoryUsage(int64_t bytes);
    void recordDiskUsage(int64_t bytes);
    void recordCPUUsage(double percentage);
    void recordConnectionCount(int64_t count);
    void recordBufferPoolHitRate(double rate);

    // Custom business metrics
    void recordCustomCounter(const std::string& name, int64_t value);
    void recordCustomHistogram(const std::string& name, double value);
    void recordCustomGauge(const std::string& name, double value);

private:
    MetricsCollector() = default;
    ~MetricsCollector() = default;

    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    bool initialized_ = false;

#ifdef HAVE_TELEMETRY
    std::shared_ptr<opentelemetry::metrics::Meter> meter_;

    // Database metrics
    std::unique_ptr<opentelemetry::metrics::Histogram<double>> query_latency_;
    std::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> query_count_;
    std::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> error_count_;
    std::unique_ptr<opentelemetry::metrics::Histogram<double>> transaction_latency_;
    std::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> transaction_count_;

    // System metrics
    std::unique_ptr<opentelemetry::metrics::UpDownCounter<int64_t>> memory_usage_;
    std::unique_ptr<opentelemetry::metrics::UpDownCounter<int64_t>> disk_usage_;
    std::unique_ptr<opentelemetry::metrics::Gauge<double>> cpu_usage_;
    std::unique_ptr<opentelemetry::metrics::UpDownCounter<int64_t>> connection_count_;
    std::unique_ptr<opentelemetry::metrics::Gauge<double>> buffer_pool_hit_rate_;
#endif

    // Internal counters for metrics that might not have telemetry
    std::atomic<uint64_t> total_queries_{0};
    std::atomic<uint64_t> total_errors_{0};
    std::atomic<uint64_t> total_transactions_{0};
    std::atomic<int64_t> current_connections_{0};
};

// Utility macros for easy metrics collection
#define RECORD_QUERY_LATENCY(type, latency) \
    latticedb::telemetry::MetricsCollector::getInstance().recordQueryLatency(type, latency)

#define INCREMENT_QUERY_COUNT(type) \
    latticedb::telemetry::MetricsCollector::getInstance().incrementQueryCount(type)

#define INCREMENT_ERROR_COUNT(type) \
    latticedb::telemetry::MetricsCollector::getInstance().incrementErrorCount(type)

#define RECORD_TRANSACTION_LATENCY(latency) \
    latticedb::telemetry::MetricsCollector::getInstance().recordTransactionLatency(latency)

#define INCREMENT_TRANSACTION_COUNT(status) \
    latticedb::telemetry::MetricsCollector::getInstance().incrementTransactionCount(status)

#define RECORD_MEMORY_USAGE(bytes) \
    latticedb::telemetry::MetricsCollector::getInstance().recordMemoryUsage(bytes)

#define RECORD_CONNECTION_COUNT(count) \
    latticedb::telemetry::MetricsCollector::getInstance().recordConnectionCount(count)

} // namespace telemetry
} // namespace latticedb