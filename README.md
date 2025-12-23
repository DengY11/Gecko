# Gecko Web Framework

A lightweight C++17 web framework with Gin-style routing, middleware chaining, and dedicated IO/worker thread pools.

## Build (CMake)
- Prereqs: CMake >= 3.16, a C++17 compiler, pthreads, and the header-only `nlohmann_json` (e.g. `nlohmann-json` package).
- Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- Build: `cmake --build build`
- Run example: `./build/example_gin_style`
- Run tests: `ctest --test-dir build` (router, HTTP parser, performance suites). The old Makefile has been replaced by CMake.

## API Quick Reference / API 快速参考
- `Engine::GET/POST/PUT/DELETE/HEAD(path, handler)` — Register HTTP routes; 注册对应的路由处理器。
- `Engine::Use(middleware)` — Add middleware `(Context&, std::function<void()>)`; 添加中间件，可调用 `next()` 继续链路。
- `Engine::Run(...)` — Start with `ServerConfig`, port, or `"host:port"`; 使用配置或端口启动服务器。
- `ServerConfig::setPort/setHost/setThreadPoolSize/setIOThreadCount/setMaxConnections/setKeepAliveTimeout/setMaxRequestBodySize/enablePerformanceMonitoring(interval)` — Fluent runtime tuning; 链式设置端口、线程数、连接数、超时、性能监控等。
- `Context` helpers — `param`, `query`, `header`, `status(code)`, `json(...)`, `string(...)`, `html(...)`, `header(key, value)`, `set/has/get` for per-request data; 路由上下文访问参数/查询/请求头，设置响应与自定义数据。

## Minimal Example / 最简示例
```cpp
#include "src/engine.hpp"

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
