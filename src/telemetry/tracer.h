#pragma once

#include <string>
#include <memory>
#include <chrono>

#ifdef HAVE_TELEMETRY
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/tracer.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#endif

namespace latticedb {
namespace telemetry {

// RAII wrapper for OpenTelemetry spans
class ScopedSpan {
public:
    explicit ScopedSpan(const std::string& operation_name);
    ScopedSpan(const std::string& operation_name, const std::string& component);
    ~ScopedSpan();

    // Disable copy constructor and assignment
    ScopedSpan(const ScopedSpan&) = delete;
    ScopedSpan& operator=(const ScopedSpan&) = delete;

    // Allow move
    ScopedSpan(ScopedSpan&& other) noexcept;
    ScopedSpan& operator=(ScopedSpan&& other) noexcept;

    void addAttribute(const std::string& key, const std::string& value);
    void addAttribute(const std::string& key, int64_t value);
    void addAttribute(const std::string& key, double value);
    void addAttribute(const std::string& key, bool value);

    void addEvent(const std::string& name);
    void addEvent(const std::string& name, const std::string& message);

    void setStatus(bool success, const std::string& description = "");
    void recordException(const std::exception& ex);

private:
#ifdef HAVE_TELEMETRY
    std::shared_ptr<opentelemetry::trace::Span> span_;
    std::unique_ptr<opentelemetry::trace::Scope> scope_;
#endif
};

// Utility macros for easy tracing
#define TRACE_OPERATION(name) \
    latticedb::telemetry::ScopedSpan trace_span(name)

#define TRACE_OPERATION_WITH_COMPONENT(name, component) \
    latticedb::telemetry::ScopedSpan trace_span(name, component)

#define TRACE_ADD_ATTRIBUTE(key, value) \
    trace_span.addAttribute(key, value)

#define TRACE_ADD_EVENT(name, message) \
    trace_span.addEvent(name, message)

#define TRACE_SET_SUCCESS() \
    trace_span.setStatus(true)

#define TRACE_SET_ERROR(description) \
    trace_span.setStatus(false, description)

#define TRACE_RECORD_EXCEPTION(ex) \
    trace_span.recordException(ex)

} // namespace telemetry
} // namespace latticedb