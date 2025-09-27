#include "tracer.h"
#include "telemetry_manager.h"
#include "../common/logger.h"

#ifdef HAVE_TELEMETRY
#include "opentelemetry/trace/semantic_conventions.h"
#include "opentelemetry/trace/status.h"
#endif

namespace latticedb {
namespace telemetry {

ScopedSpan::ScopedSpan(const std::string& operation_name) {
#ifdef HAVE_TELEMETRY
    auto& manager = TelemetryManager::getInstance();
    if (manager.isEnabled()) {
        auto tracer = manager.getTracer();
        if (tracer) {
            span_ = tracer->StartSpan(operation_name);
            scope_ = std::make_unique<opentelemetry::trace::Scope>(span_);
        }
    }
#endif
}

ScopedSpan::ScopedSpan(const std::string& operation_name, const std::string& component) {
#ifdef HAVE_TELEMETRY
    auto& manager = TelemetryManager::getInstance();
    if (manager.isEnabled()) {
        auto tracer = manager.getTracer();
        if (tracer) {
            span_ = tracer->StartSpan(operation_name);
            scope_ = std::make_unique<opentelemetry::trace::Scope>(span_);
            if (span_) {
                span_->SetAttribute("component", component);
            }
        }
    }
#endif
}

ScopedSpan::~ScopedSpan() {
#ifdef HAVE_TELEMETRY
    if (span_) {
        span_->End();
    }
#endif
}

ScopedSpan::ScopedSpan(ScopedSpan&& other) noexcept {
#ifdef HAVE_TELEMETRY
    span_ = std::move(other.span_);
    scope_ = std::move(other.scope_);
    other.span_ = nullptr;
    other.scope_ = nullptr;
#endif
}

ScopedSpan& ScopedSpan::operator=(ScopedSpan&& other) noexcept {
    if (this != &other) {
#ifdef HAVE_TELEMETRY
        if (span_) {
            span_->End();
        }
        span_ = std::move(other.span_);
        scope_ = std::move(other.scope_);
        other.span_ = nullptr;
        other.scope_ = nullptr;
#endif
    }
    return *this;
}

void ScopedSpan::addAttribute(const std::string& key, const std::string& value) {
#ifdef HAVE_TELEMETRY
    if (span_) {
        span_->SetAttribute(key, value);
    }
#endif
}

void ScopedSpan::addAttribute(const std::string& key, int64_t value) {
#ifdef HAVE_TELEMETRY
    if (span_) {
        span_->SetAttribute(key, value);
    }
#endif
}

void ScopedSpan::addAttribute(const std::string& key, double value) {
#ifdef HAVE_TELEMETRY
    if (span_) {
        span_->SetAttribute(key, value);
    }
#endif
}

void ScopedSpan::addAttribute(const std::string& key, bool value) {
#ifdef HAVE_TELEMETRY
    if (span_) {
        span_->SetAttribute(key, value);
    }
#endif
}

void ScopedSpan::addEvent(const std::string& name) {
#ifdef HAVE_TELEMETRY
    if (span_) {
        span_->AddEvent(name);
    }
#endif
}

void ScopedSpan::addEvent(const std::string& name, const std::string& message) {
#ifdef HAVE_TELEMETRY
    if (span_) {
        span_->AddEvent(name, {{"message", message}});
    }
#endif
}

void ScopedSpan::setStatus(bool success, const std::string& description) {
#ifdef HAVE_TELEMETRY
    if (span_) {
        if (success) {
            span_->SetStatus(opentelemetry::trace::StatusCode::kOk, description);
        } else {
            span_->SetStatus(opentelemetry::trace::StatusCode::kError, description);
        }
    }
#endif
}

void ScopedSpan::recordException(const std::exception& ex) {
#ifdef HAVE_TELEMETRY
    if (span_) {
        span_->AddEvent("exception", {
            {"exception.type", typeid(ex).name()},
            {"exception.message", ex.what()}
        });
        span_->SetStatus(opentelemetry::trace::StatusCode::kError, ex.what());
    }
#endif
}

} // namespace telemetry
} // namespace latticedb