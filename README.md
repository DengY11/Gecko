# Gecko Web Framework

A lightweight C++17 web framework with Gin-style routing, middleware chaining, and dedicated IO/worker thread pools.

## Architecture / 架构概览
- 主线程负责 accept + epoll；`IOThreadPool` 做异步读写/epoll；`ThreadPool` 运行业务处理与序列化。
- 路由与中间件：`Engine` 注册路由与洋葱模型中间件，`Router` 做静态/参数匹配。
- 可选协作式调度：`enableCooperativeScheduling` 将任务切片运行，支持优先级、时间片、最大切片次数与超时，避免长任务阻塞。
- 性能监控：周期性打印连接数、QPS、队列深度、协作重排/丢弃计数。
- 可扩展组件：预留 `Engine::Static` 静态文件服务；`ServerConfig` 链式调优端口、线程、背压阈值等。

## Modules / 模块划分
- `http/`：HTTP 引擎、路由、请求响应对象、线程池与服务器配置。
- `logger/`：独立的异步日志模块，可选择控制台/文件/双写输出。
- `rpc/`：可选的 gRPC `RpcServer` 封装，支持 Server Interceptor 风格的中间件（示例：JWT/metadata 校验）。
- `tracing/`：轻量级链路追踪器与 HTTP Trace 中间件，生成 trace/span id 并可自定义导出。

## Build (CMake)
- Prereqs: CMake >= 3.16, a C++17 compiler, pthreads, and the header-only `nlohmann_json` (e.g. `nlohmann-json` package). For gRPC example, install `gRPC` + `protobuf` with `protoc`.
- Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release [-DGECKO_ENABLE_GRPC=ON]`
- Build: `cmake --build build`
- Run HTTP example: `./build/example_gin_style`
- Run HTTP + gRPC middlewares example (requires gRPC/protoc): `./build/example_http_rpc_middleware`
- Run tests: `ctest --test-dir build` (router, HTTP parser, performance suites). The old Makefile has been replaced by CMake.

## RPC (gRPC) Support / RPC 支持
- Optional gRPC layer: configure with `-DGECKO_ENABLE_GRPC=ON` (requires `gRPC::grpc++` and `gRPC::grpc++_reflection`).
- `RpcServer` wraps `grpc::ServerBuilder` and can run alongside the HTTP engine to build microservices.
- Example: start gRPC in the background, keep HTTP for REST/metrics:
```cpp
#include "http/engine.hpp"
#include "rpc/rpc_server.hpp"
#include "helloworld.grpc.pb.h"

class GreeterService final : public helloworld::Greeter::Service {
public:
    grpc::Status SayHello(grpc::ServerContext*,
                          const helloworld::HelloRequest* req,
                          helloworld::HelloReply* resp) override {
        resp->set_message("Hello " + req->name());
        return grpc::Status::OK;
    }
};

int main() {
    Gecko::Engine http;
    http.GET("/healthz", [](Gecko::Context& ctx) { ctx.json({{"status", "ok"}}); });

    GreeterService greeter;
    Gecko::RpcServer rpc(Gecko::RpcConfig().setAddress("0.0.0.0:50051"));
    rpc.AddService(&greeter).StartBackground();  // gRPC stays alive while HTTP runs

    Gecko::ServerConfig httpCfg;
    http.Run(httpCfg);
    rpc.Shutdown();
}
```

## API Quick Reference / API 快速参考
- `Engine::GET/POST/PUT/DELETE/HEAD/PATCH/OPTIONS(path, handler)` or `Engine::AddRoute(method, path, handler)` — Register HTTP routes; 注册对应的路由处理器。
- `Engine::Use(middleware)` — Add middleware `(Context&, std::function<void()>)`; 添加中间件，可调用 `next()` 继续链路。
- `Engine::Run(...)` — Start with `ServerConfig`, port, or `"host:port"`; 使用配置或端口启动服务器。
- `ServerConfig::setPort/setHost/setThreadPoolSize/setIOThreadCount/setMaxConnections/setKeepAliveTimeout/setMaxRequestBodySize/enablePerformanceMonitoring(interval)/enableCooperativeScheduling(timeSliceMs, priority, maxSlices, timeoutMs)` — Fluent runtime tuning; 链式设置端口、线程数、连接数、超时、性能监控、协作式调度等。
- `Context` helpers — `param`, `query`, `header`, `status(code)`, `json(...)`, `string(...)`, `html(...)`, `header(key, value)`, `set/has/get` for per-request data; 路由上下文访问参数/查询/请求头，设置响应与自定义数据。

## Minimal Example / 最简示例
```cpp
#include "http/engine.hpp"

int main() {
    Gecko::Engine app;
    app.GET("/hello/:name", [](Gecko::Context& ctx) {
        ctx.status(200).json({{"message", "Hello " + ctx.param("name")}});
    });

    Gecko::ServerConfig config = Gecko::ServerConfig()
        .setPort(8080)
        .setThreadPoolSize(4)
        .setIOThreadCount(4)
        .enablePerformanceMonitoring(5);

    app.Run(config);
}
```

## Example Endpoints / 示例接口
- `GET /ping` — JSON health check; 健康检查返回 JSON。
- `GET /hello/:name` — Path parameter echo; 演示路径参数。
- `GET /search?q=...&type=...` — Query parsing showcase; 查询参数解析示例。
- `GET /error-test?simulate=error` — Triggers logging + 500; 用于演示错误日志。
- `GET /api/users/:id` — Simple REST-style user lookup; REST 风格查询示例。
