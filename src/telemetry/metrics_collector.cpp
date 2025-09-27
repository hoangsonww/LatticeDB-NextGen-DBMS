#include "metrics_collector.h"
#include "telemetry_manager.h"
#include "../common/logger.h"

#ifdef HAVE_TELEMETRY
#include "opentelemetry/common/key_value_iterable.h"
#endif

namespace latticedb {
namespace telemetry {

MetricsCollector& MetricsCollector::getInstance() {
    static MetricsCollector instance;
    return instance;
}

void MetricsCollector::initialize() {
    if (initialized_) return;

#ifdef HAVE_TELEMETRY
    auto& manager = TelemetryManager::getInstance();
    if (manager.isEnabled()) {
        try {
            meter_ = manager.getMeter();

            if (meter_) {
                // Database operation metrics
                query_latency_ = meter_->CreateDoubleHistogram(
                    "latticedb_query_duration_milliseconds",
                    "Time taken to execute database queries",
                    "ms"
                );

                query_count_ = meter_->CreateUInt64Counter(
                    "latticedb_queries_total",
                    "Total number of database queries executed"
                );

                error_count_ = meter_->CreateUInt64Counter(
                    "latticedb_errors_total",
                    "Total number of errors encountered"
                );

                transaction_latency_ = meter_->CreateDoubleHistogram(
                    "latticedb_transaction_duration_milliseconds",
                    "Time taken to complete database transactions",
                    "ms"
                );

                transaction_count_ = meter_->CreateUInt64Counter(
                    "latticedb_transactions_total",
                    "Total number of database transactions"
                );

                // System metrics
                memory_usage_ = meter_->CreateInt64UpDownCounter(
                    "latticedb_memory_usage_bytes",
                    "Current memory usage in bytes",
                    "bytes"
                );

                disk_usage_ = meter_->CreateInt64UpDownCounter(
                    "latticedb_disk_usage_bytes",
                    "Current disk usage in bytes",
                    "bytes"
                );

                cpu_usage_ = meter_->CreateDoubleGauge(
                    "latticedb_cpu_usage_percent",
                    "Current CPU usage percentage",
                    "%"
                );

                connection_count_ = meter_->CreateInt64UpDownCounter(
                    "latticedb_connections_active",
                    "Number of active database connections"
                );

                buffer_pool_hit_rate_ = meter_->CreateDoubleGauge(
                    "latticedb_buffer_pool_hit_rate",
                    "Buffer pool hit rate percentage",
                    "%"
                );

                Logger::get_instance()->info("Metrics collector initialized with OpenTelemetry");
            }
        } catch (const std::exception& e) {
            Logger::get_instance()->error("Failed to initialize metrics collector: {}", e.what());
        }
    }
#endif

    initialized_ = true;
    Logger::get_instance()->info("Metrics collector initialized");
}

void MetricsCollector::recordQueryLatency(const std::string& query_type, double latency_ms) {
#ifdef HAVE_TELEMETRY
    if (query_latency_) {
        query_latency_->Record(latency_ms, {{"query_type", query_type}});
    }
#endif
    total_queries_++;
}

void MetricsCollector::incrementQueryCount(const std::string& query_type) {
#ifdef HAVE_TELEMETRY
    if (query_count_) {
        query_count_->Add(1, {{"query_type", query_type}});
    }
#endif
    total_queries_++;
}

void MetricsCollector::incrementErrorCount(const std::string& error_type) {
#ifdef HAVE_TELEMETRY
    if (error_count_) {
        error_count_->Add(1, {{"error_type", error_type}});
    }
#endif
    total_errors_++;
}

void MetricsCollector::recordTransactionLatency(double latency_ms) {
#ifdef HAVE_TELEMETRY
    if (transaction_latency_) {
        transaction_latency_->Record(latency_ms);
    }
#endif
}

void MetricsCollector::incrementTransactionCount(const std::string& status) {
#ifdef HAVE_TELEMETRY
    if (transaction_count_) {
        transaction_count_->Add(1, {{"status", status}});
    }
#endif
    total_transactions_++;
}

void MetricsCollector::recordMemoryUsage(int64_t bytes) {
#ifdef HAVE_TELEMETRY
    if (memory_usage_) {
        memory_usage_->Add(bytes);
    }
#endif
}

void MetricsCollector::recordDiskUsage(int64_t bytes) {
#ifdef HAVE_TELEMETRY
    if (disk_usage_) {
        disk_usage_->Add(bytes);
    }
#endif
}

void MetricsCollector::recordCPUUsage(double percentage) {
#ifdef HAVE_TELEMETRY
    if (cpu_usage_) {
        cpu_usage_->Record(percentage);
    }
#endif
}

void MetricsCollector::recordConnectionCount(int64_t count) {
#ifdef HAVE_TELEMETRY
    if (connection_count_) {
        connection_count_->Add(count - current_connections_.load());
    }
#endif
    current_connections_ = count;
}

void MetricsCollector::recordBufferPoolHitRate(double rate) {
#ifdef HAVE_TELEMETRY
    if (buffer_pool_hit_rate_) {
        buffer_pool_hit_rate_->Record(rate);
    }
#endif
}

void MetricsCollector::recordCustomCounter(const std::string& name, int64_t value) {
#ifdef HAVE_TELEMETRY
    if (meter_) {
        auto counter = meter_->CreateInt64Counter(name, "Custom counter metric");
        counter->Add(value);
    }
#endif
}

void MetricsCollector::recordCustomHistogram(const std::string& name, double value) {
#ifdef HAVE_TELEMETRY
    if (meter_) {
        auto histogram = meter_->CreateDoubleHistogram(name, "Custom histogram metric");
        histogram->Record(value);
    }
#endif
}

void MetricsCollector::recordCustomGauge(const std::string& name, double value) {
#ifdef HAVE_TELEMETRY
    if (meter_) {
        auto gauge = meter_->CreateDoubleGauge(name, "Custom gauge metric");
        gauge->Record(value);
    }
#endif
}

} // namespace telemetry
} // namespace latticedb