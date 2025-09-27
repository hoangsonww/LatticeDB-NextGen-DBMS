#include "telemetry_manager.h"
#include "../common/logger.h"

#ifdef HAVE_TELEMETRY
#include "opentelemetry/exporters/jaeger/jaeger_exporter_factory.h"
#include "opentelemetry/exporters/prometheus/prometheus_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/exporters/prometheus/prometheus_exporter_factory.h"
#include "opentelemetry/metrics/provider.h"
#endif

namespace latticedb {
namespace telemetry {

TelemetryManager& TelemetryManager::getInstance() {
    static TelemetryManager instance;
    return instance;
}

void TelemetryManager::initialize(const TelemetryConfig& config) {
    config_ = config;

    if (!config_.enabled) {
        Logger::get_instance()->info("Telemetry is disabled");
        return;
    }

#ifdef HAVE_TELEMETRY
    try {
        Logger::get_instance()->info("Initializing telemetry with Jaeger endpoint: {}", config_.jaeger_endpoint);

        // Create resource
        auto resource = opentelemetry::sdk::resource::Resource::Create({
            {opentelemetry::sdk::resource::SemanticConventions::kServiceName, config_.service_name},
            {opentelemetry::sdk::resource::SemanticConventions::kServiceVersion, config_.service_version},
            {opentelemetry::sdk::resource::SemanticConventions::kServiceInstanceId, "latticedb-instance-1"}
        });

        // Initialize Jaeger exporter for traces
        opentelemetry::exporter::jaeger::JaegerExporterOptions jaeger_opts;
        jaeger_opts.endpoint = config_.jaeger_endpoint;
        auto jaeger_exporter = opentelemetry::exporter::jaeger::JaegerExporterFactory::Create(jaeger_opts);

        // Create batch span processor
        auto processor = opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
            std::move(jaeger_exporter), {});

        // Create tracer provider
        tracer_provider_ = opentelemetry::sdk::trace::TracerProviderFactory::Create(
            std::move(processor), resource);

        // Set the global tracer provider
        opentelemetry::trace::Provider::SetTracerProvider(tracer_provider_);

        // Get tracer
        tracer_ = opentelemetry::trace::Provider::GetTracerProvider()->GetTracer(
            config_.service_name, config_.service_version);

        // Initialize Prometheus exporter for metrics
        opentelemetry::exporter::prometheus::PrometheusExporterOptions prometheus_opts;
        prometheus_opts.url = config_.prometheus_endpoint;
        auto prometheus_exporter = opentelemetry::exporter::prometheus::PrometheusExporterFactory::Create(prometheus_opts);

        // Create periodic metric reader
        auto metric_reader = opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
            std::move(prometheus_exporter), {});

        // Create meter provider
        meter_provider_ = opentelemetry::sdk::metrics::MeterProviderFactory::Create(
            std::move(metric_reader), resource);

        // Set the global meter provider
        opentelemetry::metrics::Provider::SetMeterProvider(meter_provider_);

        // Get meter
        meter_ = opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter(
            config_.service_name, config_.service_version);

        enabled_ = true;
        Logger::get_instance()->info("Telemetry initialized successfully");
    } catch (const std::exception& e) {
        Logger::get_instance()->error("Failed to initialize telemetry: {}", e.what());
        enabled_ = false;
    }
#else
    Logger::get_instance()->warn("Telemetry requested but not compiled with HAVE_TELEMETRY support");
    enabled_ = false;
#endif
}

void TelemetryManager::shutdown() {
    if (!enabled_) return;

#ifdef HAVE_TELEMETRY
    try {
        // Flush any remaining spans/metrics
        if (tracer_provider_) {
            static_cast<opentelemetry::sdk::trace::TracerProvider*>(tracer_provider_.get())->ForceFlush(std::chrono::milliseconds(5000));
        }

        if (meter_provider_) {
            static_cast<opentelemetry::sdk::metrics::MeterProvider*>(meter_provider_.get())->ForceFlush(std::chrono::milliseconds(5000));
        }

        Logger::get_instance()->info("Telemetry shutdown completed");
    } catch (const std::exception& e) {
        Logger::get_instance()->error("Error during telemetry shutdown: {}", e.what());
    }
#endif

    enabled_ = false;
}

#ifdef HAVE_TELEMETRY
std::shared_ptr<opentelemetry::trace::Tracer> TelemetryManager::getTracer() const {
    return tracer_;
}

std::shared_ptr<opentelemetry::metrics::Meter> TelemetryManager::getMeter() const {
    return meter_;
}
#endif

} // namespace telemetry
} // namespace latticedb