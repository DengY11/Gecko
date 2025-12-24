#include "tracing/tracer.hpp"

#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

namespace Gecko::Tracing {

namespace {
std::string formatDurationMs(const SpanRecord& record) {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        record.end_time - record.start_time);
    std::ostringstream oss;
    oss << duration.count();
    return oss.str();
}

std::string defaultFormat(const SpanRecord& record) {
    std::ostringstream oss;
    oss << "[trace] trace_id=" << record.context.trace_id
        << " span_id=" << record.context.span_id;
    if (!record.context.parent_span_id.empty()) {
        oss << " parent_span_id=" << record.context.parent_span_id;
    }
    oss << " name=\"" << record.name << "\""
        << " status=" << (record.status.empty() ? "OK" : record.status)
        << " duration_ms=" << formatDurationMs(record);
    if (!record.tags.empty()) {
        oss << " tags={";
        bool first = true;
        for (const auto& [k, v] : record.tags) {
            if (!first) {
                oss << ", ";
            }
            first = false;
            oss << k << ":" << v;
        }
        oss << "}";
    }
    return oss.str();
}
} // namespace

Tracer::Tracer(Exporter exporter) {
    if (exporter) {
        exporter_ = std::move(exporter);
    } else {
        exporter_ = [](const SpanRecord& record) {
            std::cout << defaultFormat(record) << std::endl;
        };
    }
}

ScopedSpan Tracer::startSpan(const std::string& name,
                             const std::string& trace_id,
                             const std::string& parent_span_id) {
    return ScopedSpan(*this, name, trace_id, parent_span_id);
}

void Tracer::setExporter(Exporter exporter) {
    std::lock_guard<std::mutex> lock(exporter_mutex_);
    exporter_ = std::move(exporter);
}

std::string Tracer::generateId() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << dist(rng);
    return oss.str();
}

void Tracer::closeSpan(SpanRecord&& record) {
    Exporter exporter_copy;
    {
        std::lock_guard<std::mutex> lock(exporter_mutex_);
        exporter_copy = exporter_;
    }
    if (exporter_copy) {
        exporter_copy(record);
    }
}

ScopedSpan::ScopedSpan(Tracer& tracer,
                       std::string name,
                       std::string trace_id,
                       std::string parent_span_id)
    : tracer_(&tracer) {
    record_.name = std::move(name);
    record_.context.trace_id = trace_id.empty() ? Tracer::generateId() : std::move(trace_id);
    record_.context.span_id = Tracer::generateId();
    record_.context.parent_span_id = std::move(parent_span_id);
    record_.start_time = std::chrono::steady_clock::now();
    record_.status = "OK";
}

ScopedSpan::ScopedSpan(ScopedSpan&& other) noexcept
    : tracer_(other.tracer_), record_(std::move(other.record_)), closed_(other.closed_) {
    other.closed_ = true;
    other.tracer_ = nullptr;
}

ScopedSpan& ScopedSpan::operator=(ScopedSpan&& other) noexcept {
    if (this != &other) {
        if (!closed_) {
            finish();
        }
        tracer_ = other.tracer_;
        record_ = std::move(other.record_);
        closed_ = other.closed_;
        other.closed_ = true;
        other.tracer_ = nullptr;
    }
    return *this;
}

ScopedSpan::~ScopedSpan() {
    if (!closed_) {
        finish();
    }
}

void ScopedSpan::setTag(const std::string& key, const std::string& value) {
    record_.tags[key] = value;
}

void ScopedSpan::setStatus(const std::string& status) {
    record_.status = status;
}

void ScopedSpan::finish() {
    record_.end_time = std::chrono::steady_clock::now();
    closed_ = true;
    if (tracer_) {
        tracer_->closeSpan(std::move(record_));
    }
}

} // namespace Gecko::Tracing
