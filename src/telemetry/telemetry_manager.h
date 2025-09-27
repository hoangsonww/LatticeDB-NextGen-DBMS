#pragma once

#include <memory>
#include <string>

#ifdef HAVE_TELEMETRY
#include "opentelemetry/exporters/jaeger/jaeger_exporter_factory.h"
#include "opentelemetry/exporters/prometheus/prometheus_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/aggregation/default_aggregation.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#endif

namespace latticedb {
namespace telemetry {

struct TelemetryConfig {
    bool enabled = false;
    std::string jaeger_endpoint = "http://localhost:14268/api/traces";
    std::string prometheus_endpoint = "localhost:9464";
    std::string otlp_endpoint = "http://localhost:4318/v1/traces";
    std::string service_name = "latticedb";
    std::string service_version = "2.0.0";
};

class TelemetryManager {
public:
    static TelemetryManager& getInstance();

    void initialize(const TelemetryConfig& config);
    void shutdown();

    bool isEnabled() const { return enabled_; }

#ifdef HAVE_TELEMETRY
    std::shared_ptr<opentelemetry::trace::Tracer> getTracer() const;
    std::shared_ptr<opentelemetry::metrics::Meter> getMeter() const;
#endif

private:
    TelemetryManager() = default;
    ~TelemetryManager() = default;

    TelemetryManager(const TelemetryManager&) = delete;
    TelemetryManager& operator=(const TelemetryManager&) = delete;

    bool enabled_ = false;
    TelemetryConfig config_;

#ifdef HAVE_TELEMETRY
    std::shared_ptr<opentelemetry::trace::TracerProvider> tracer_provider_;
    std::shared_ptr<opentelemetry::metrics::MeterProvider> meter_provider_;
    std::shared_ptr<opentelemetry::trace::Tracer> tracer_;
    std::shared_ptr<opentelemetry::metrics::Meter> meter_;
#endif
};

} // namespace telemetry
} // namespace latticedb