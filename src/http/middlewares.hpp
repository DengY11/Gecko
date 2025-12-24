#ifndef MIDDLEWARES_H
#define MIDDLEWARES_H
#include "context.hpp"
#include "tracing/tracer.hpp"
#include <memory>
#include <string>

namespace Gecko {

class GeckoMiddleware {
public:
    static void CORS(Context &ctx, std::function<void()> next) {
        ctx.header("Access-Control-Allow-Origin", "*");
        ctx.header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
        ctx.header("Access-Control-Allow-Headers", "Content-Type");
        next();
    }

    static std::function<void(Context&, std::function<void()>)>
    RequestID(const std::string& header_name = "X-Request-ID") {
        return [header_name](Context& ctx, std::function<void()> next) {
            std::string request_id;
            if (ctx.has("request_id")) {
                request_id = ctx.get<std::string>("request_id");
            } else {
                request_id = Tracing::Tracer::generateId();
                ctx.set("request_id", request_id);
            }
            ctx.header(header_name, request_id);
            next();
        };
    }

    static std::function<void(Context&, std::function<void()>)>
    ServerHeader(const std::string& server_name = "Gecko") {
        return [server_name](Context& ctx, std::function<void()> next) {
            ctx.header("Server", server_name);
            next();
        };
    }

    static std::function<void(Context&, std::function<void()>)>
    AuthBearer(const std::string& token,
               const std::string& scheme = "Bearer",
               int deny_status = 401) {
        return [token, scheme, deny_status](Context& ctx, std::function<void()> next) {
            std::string auth = ctx.header("Authorization");
            std::string prefix = scheme + " ";
            if (auth.rfind(prefix, 0) != 0) {
                ctx.status(deny_status).string("Unauthorized");
                return;
            }
            std::string presented = auth.substr(prefix.size());
            if (presented != token) {
                ctx.status(deny_status).string("Unauthorized");
                return;
            }
            next();
        };
    }

    static std::function<void(Context&, std::function<void()>)>
    Trace(std::shared_ptr<Tracing::Tracer> tracer) {
        return [tracer](Context& ctx, std::function<void()> next) {
            if (!tracer) {
                next();
                return;
            }

            auto span = tracer->startSpan(ctx.request().getUrl());
            span.setTag("component", "http");
            span.setTag("http.method", methodToString(ctx.request().getMethod()));
            span.setTag("http.target", ctx.request().getUrl());

            ctx.header("X-Trace-Id", span.context().trace_id);
            ctx.set("trace_id", span.context().trace_id);

            next();

            span.setTag("http.status_code", std::to_string(ctx.response().getStatusCode()));
            span.setStatus(std::to_string(ctx.response().getStatusCode()));
        };
    }

private:
    static std::string methodToString(HttpMethod method) {
        switch (method) {
            case HttpMethod::GET: return "GET";
            case HttpMethod::POST: return "POST";
            case HttpMethod::PUT: return "PUT";
            case HttpMethod::DELETE: return "DELETE";
            case HttpMethod::PATCH: return "PATCH";
            case HttpMethod::OPTIONS: return "OPTIONS";
            case HttpMethod::HEAD: return "HEAD";
            default: return "UNKNOWN";
        }
    }
};

} // namespace Gecko

#endif
