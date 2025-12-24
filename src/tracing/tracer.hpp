#ifndef GECKO_TRACING_TRACER_HPP
#define GECKO_TRACING_TRACER_HPP

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Gecko {
namespace Tracing {

struct SpanContext {
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
};

struct SpanRecord {
    SpanContext context;
    std::string name;
    std::unordered_map<std::string, std::string> tags;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    std::string status;
};

class Tracer;

class ScopedSpan {
public:
    ScopedSpan(Tracer& tracer,
               std::string name,
               std::string trace_id = "",
               std::string parent_span_id = "");
    ScopedSpan(const ScopedSpan&) = delete;
    ScopedSpan& operator=(const ScopedSpan&) = delete;
    ScopedSpan(ScopedSpan&& other) noexcept;
    ScopedSpan& operator=(ScopedSpan&& other) noexcept;
    ~ScopedSpan();

    const SpanContext& context() const { return record_.context; }
    void setTag(const std::string& key, const std::string& value);
    void setStatus(const std::string& status);

private:
    void finish();

    Tracer* tracer_;
    SpanRecord record_;
    bool closed_{false};
};

class Tracer {
public:
    using Exporter = std::function<void(const SpanRecord&)>;

    explicit Tracer(Exporter exporter = {});

    ScopedSpan startSpan(const std::string& name,
                         const std::string& trace_id = "",
                         const std::string& parent_span_id = "");

    void setExporter(Exporter exporter);

    static std::string generateId();

private:
    friend class ScopedSpan;
    void closeSpan(SpanRecord&& record);

    Exporter exporter_;
    std::mutex exporter_mutex_;
};

} // namespace Tracing
} // namespace Gecko

#endif
