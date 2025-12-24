#include "http/engine.hpp"
#include "http/middlewares.hpp"
#include "logger/logger.hpp"
#include "tracing/tracer.hpp"
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>

#ifdef GECKO_ENABLE_GRPC
#include "rpc/middlewares.hpp"
#include "rpc/rpc_server.hpp"
#include "greeter.grpc.pb.h"
#endif

int main() {
    auto tracer = std::make_shared<Gecko::Tracing::Tracer>();
    Gecko::Logger http_log(Gecko::LogLevel::INFO, 1, Gecko::LogOutput::CONSOLE);

    Gecko::Engine app;
    app.Use(Gecko::GeckoMiddleware::RequestID());
    app.Use(Gecko::GeckoMiddleware::Trace(tracer));
    app.Use(Gecko::GeckoMiddleware::ServerHeader("Gecko/Modular"));
    app.Use(Gecko::GeckoMiddleware::AuthBearer("secret-token"));

    app.GET("/ping", [](Gecko::Context& ctx) {
        std::string trace_id = ctx.has("trace_id") ? ctx.get<std::string>("trace_id") : "";
        std::string request_id = ctx.has("request_id") ? ctx.get<std::string>("request_id") : "";
        ctx.json({
            {"message", "pong"},
            {"trace_id", trace_id},
            {"request_id", request_id},
        });
    });

    app.GET("/hello/:name", [&http_log](Gecko::Context& ctx) {
        std::string name = ctx.param("name");
        http_log.log(Gecko::LogLevel::INFO, "Hello route for " + name);
        ctx.json({{"message", "Hello " + name},
                  {"request_id", ctx.has("request_id") ? ctx.get<std::string>("request_id") : ""}});
    });

    Gecko::ServerConfig http_cfg;
    http_cfg.setPort(8080).setThreadPoolSize(4).setIOThreadCount(2);

#ifdef GECKO_ENABLE_GRPC
    class GreeterService final : public greeter::Greeter::Service {
    public:
        explicit GreeterService(std::shared_ptr<Gecko::Tracing::Tracer> tracer)
            : tracer_(std::move(tracer)) {}

        grpc::Status SayHello(grpc::ServerContext* context,
                              const greeter::HelloRequest* request,
                              greeter::HelloReply* response) override {
            (void)context;
            auto span = tracer_->startSpan("grpc/SayHello");
            span.setTag("rpc.system", "grpc");
            span.setTag("rpc.method", "SayHello");

            response->set_message("Hello " + request->name());
            response->set_trace_id(span.context().trace_id);

            return grpc::Status::OK;
        }

    private:
        std::shared_ptr<Gecko::Tracing::Tracer> tracer_;
    };

    GreeterService greeter(tracer);
    Gecko::RpcServer rpc(Gecko::RpcConfig().setAddress("0.0.0.0:50051"));
    rpc.AddMiddleware(Gecko::RpcMiddlewares::RequireJwtToken("secret-token"));
    rpc.AddService(&greeter).StartBackground();
    std::cout << "[rpc] listening on 0.0.0.0:50051 (JWT Bearer secret-token)" << std::endl;
#else
    std::cout << "[rpc] build without GECKO_ENABLE_GRPC; gRPC example disabled" << std::endl;
#endif

    std::cout << "[http] listening on 0.0.0.0:" << http_cfg.port
              << " with Trace + RequestID + AuthBearer middleware" << std::endl;
    app.Run(http_cfg);

#ifdef GECKO_ENABLE_GRPC
    rpc.Shutdown();
#endif

    return 0;
}
