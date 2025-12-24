#include "http/engine.hpp"
#include "logger/logger.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <nlohmann/json.hpp>

int main() {
    try {
        std::cout << "Gecko Web Framework - Gin-style API + Logger demo" << std::endl;
        std::cout << "=================================================" << std::endl;

        Gecko::Logger access_logger(Gecko::LogLevel::ERROR, 2, Gecko::LogOutput::BOTH, "access.log");
        Gecko::Logger error_logger(Gecko::LogLevel::ERROR, 1, Gecko::LogOutput::FILE, "error.log");
        Gecko::Logger debug_logger(Gecko::LogLevel::ERROR, 1, Gecko::LogOutput::CONSOLE);

        Gecko::Engine app;

        app.Use([&access_logger, &debug_logger](Gecko::Context& ctx, std::function<void()> next) {
            auto start = std::chrono::high_resolution_clock::now();
            std::string client_info = "IP: " + ctx.header("X-Forwarded-For") +
                                    " UserAgent: " + ctx.header("User-Agent");

            access_logger.log(Gecko::LogLevel::INFO,
                "Request started: " + ctx.request().getUrl() + " | " + client_info);

            debug_logger.log(Gecko::LogLevel::DEBUG,
                "Processing request on thread: " + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));

            next();

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            access_logger.log(Gecko::LogLevel::INFO,
                "Request completed: " + ctx.request().getUrl() + " | Duration: " +
                std::to_string(duration.count()) + "μs");
        });

        app.Use([&debug_logger](Gecko::Context& ctx, std::function<void()> next) {
            debug_logger.log(Gecko::LogLevel::DEBUG, "Applying CORS headers");
            ctx.header("Access-Control-Allow-Origin", "*")
               .header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE")
               .header("Access-Control-Allow-Headers", "Content-Type");
            next();
        });

        app.GET("/", [&access_logger](Gecko::Context& ctx) {
            access_logger.log(Gecko::LogLevel::INFO, "Serving homepage");
            ctx.html(R"(<!DOCTYPE html>
<html>
<head>
    <title>Gecko Gecko Web Framework + Logger</title>
    <meta charset="utf-8">
    <style>
        body { font-family: 'Segoe UI', sans-serif; margin: 40px; background: #f5f5f5; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #2e7d32; text-align: center; }
        .endpoint { margin: 15px 0; padding: 15px; background: #f8f9fa; border-left: 4px solid #2e7d32; border-radius: 5px; }
        a { color: #1976d2; text-decoration: none; font-weight: 500; }
        a:hover { text-decoration: underline; }
        .badge { background: #2e7d32; color: white; padding: 2px 6px; border-radius: 3px; font-size: 12px; }
        .logger-info { background: #e3f2fd; padding: 15px; border-radius: 5px; margin: 20px 0; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Gecko Gecko Web Framework + Logger</h1>
        <p><strong>现在支持Gin风格的API、多线程处理和独立的Logger系统！</strong></p>

        <div class="logger-info">
            <h3>[LOG] Logger系统特性:</h3>
            <ul>
                <li>[OK] <strong>多输出目标</strong> - 控制台、文件、或同时输出</li>
                <li>[OK] <strong>多线程安全</strong> - 异步日志写入</li>
                <li>[OK] <strong>分级日志</strong> - DEBUG、INFO、WARN、ERROR、FATAL</li>
                <li>[OK] <strong>时间戳格式化</strong> - 精确到毫秒</li>
                <li>[OK] <strong>独立库设计</strong> - 用户可选择使用</li>
                <li>[OK] <strong>配置灵活</strong> - 支持运行时修改输出目标</li>
            </ul>
            <p><em>检查服务器控制台和 access.log、error.log 文件查看日志输出效果！</em></p>
        </div>

        <h2>[GOAL] API端点：</h2>
        <div class="endpoint">
            <span class="badge">GET</span> <a href="/ping">/ping</a> - JSON响应测试
        </div>
        <div class="endpoint">
            <span class="badge">GET</span> <a href="/hello/world">/hello/:name</a> - 路径参数测试
        </div>
        <div class="endpoint">
            <span class="badge">GET</span> <a href="/search?q=gecko&type=framework">/search</a> - 查询参数测试
        </div>
        <div class="endpoint">
            <span class="badge">GET</span> <a href="/error-test">/error-test</a> - 错误日志测试
        </div>
        <div class="endpoint">
            <span class="badge">GET</span> <a href="/api/users/123">/api/users/:id</a> - RESTful API示例
        </div>
    </div>
</body>
</html>)");
        });

        app.GET("/ping", [&access_logger, &debug_logger](Gecko::Context& ctx) {
            debug_logger.log(Gecko::LogLevel::DEBUG, "Ping endpoint called");

            nlohmann::json response = {
                {"message", "pong"},
                {"framework", "Gecko Web Framework"},
                {"status", "running"},
                {"style", "gin-like"},
                {"logger_enabled", true},
                {"thread_id", std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()))},
                {"features", nlohmann::json::array({
                    "context-based handlers",
                    "onion-model middleware",
                    "method chaining",
                    "independent logger system",
                    "multi-threading",
                    "configurable server"
                })}
            };

            access_logger.log(Gecko::LogLevel::INFO, "Ping response sent successfully");
            ctx.json(response);
        });

        app.GET("/hello/:name", [&access_logger, &debug_logger](Gecko::Context& ctx) {
            std::string name = ctx.param("name");
            debug_logger.log(Gecko::LogLevel::DEBUG, "Hello endpoint called with name: " + name);

            if (name.empty()) {
                access_logger.log(Gecko::LogLevel::WARN, "Hello endpoint called without name parameter");
                ctx.status(400).json({{"error", "name parameter is required"}});
                return;
            }

            nlohmann::json response = {
                {"message", "Hello, " + name + "!"},
                {"path_param", name},
                {"framework", "Gecko"},
                {"logged_by", "Independent Logger System"},
                {"thread_id", std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()))}
            };

            access_logger.log(Gecko::LogLevel::INFO, "Hello response sent for: " + name);
            ctx.status(200).json(response);
        });

        app.GET("/search", [&access_logger, &debug_logger](Gecko::Context& ctx) {
            std::string query = ctx.query("q");
            std::string type = ctx.query("type");

            debug_logger.log(Gecko::LogLevel::DEBUG,
                "Search endpoint: query=" + query + ", type=" + type);

            nlohmann::json response = {
                {"search_query", query},
                {"search_type", type},
                {"results", nlohmann::json::array()},
                {"total", 0},
                {"logged_by", "Gecko Logger System"},
                {"thread_id", std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()))}
            };

            if (!query.empty()) {
                response["results"] = nlohmann::json::array({
                    {{"id", 1}, {"title", "Gecko Web Framework"}, {"description", "A fast C++ web framework with logging"}},
                    {{"id", 2}, {"title", "Gecko Logger"}, {"description", "Independent logging system for C++"}}
                });
                response["total"] = 2;
                access_logger.log(Gecko::LogLevel::INFO,
                    "Search performed: query=" + query + ", results=2");
            } else {
                access_logger.log(Gecko::LogLevel::INFO, "Empty search query received");
            }

            ctx.json(response);
        });

        app.GET("/error-test", [&error_logger, &debug_logger](Gecko::Context& ctx) {
            debug_logger.log(Gecko::LogLevel::DEBUG, "Error test endpoint called");

            try {
                std::string test_param = ctx.query("simulate");
                if (test_param == "error") {
                    throw std::runtime_error("This is a simulated error for testing error logging");
                }

                nlohmann::json response = {
                    {"message", "Error test endpoint"},
                    {"status", "success"},
                    {"note", "Add ?simulate=error to trigger error logging"},
                    {"thread_id", std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()))}
                };
                ctx.json(response);

            } catch (const std::exception& e) {
                error_logger.log(Gecko::LogLevel::ERROR,
                    "Error in /error-test: " + std::string(e.what()));

                ctx.status(500).json({
                    {"error", "Internal server error"},
                    {"message", e.what()},
                    {"logged_to", "error.log"},
                    {"thread_id", std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()))}
                });
            }
        });

        app.GET("/api/users/:id", [&access_logger, &debug_logger](Gecko::Context& ctx) {
            std::string userId = ctx.param("id");
            debug_logger.log(Gecko::LogLevel::DEBUG, "User API called for ID: " + userId);

            if (userId == "123") {
                nlohmann::json user = {
                    {"id", 123},
                    {"name", "张三"},
                    {"email", "zhangsan@example.com"},
                    {"role", "admin"},
                    {"created_at", "2024-01-25T12:00:00Z"},
                    {"processed_by_thread", std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()))}
                };
                access_logger.log(Gecko::LogLevel::INFO, "User found: " + userId);
                ctx.json(user);
            } else if (userId == "赵敏") {
                nlohmann::json user = {
                    {"id", 456},
                    {"name", "赵敏"},
                    {"email", "zhaomin@qq.com"},
                    {"role", "user"},
                    {"created_at", "2024-01-20T08:30:00Z"},
                    {"processed_by_thread", std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()))}
                };
                access_logger.log(Gecko::LogLevel::INFO, "User found: " + userId);
                ctx.json(user);
            } else {
                access_logger.log(Gecko::LogLevel::WARN, "User not found: " + userId);
                ctx.status(404).json({
                    {"error", "User not found"},
                    {"user_id", userId},
                    {"thread_id", std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()))}
                });
            }
        });

        std::cout << "\n[START] Preparing server launch parameters..." << std::endl;

        size_t max_threads = std::thread::hardware_concurrency();
        if (max_threads == 0) {
            max_threads = 8;
        }

        Gecko::ServerConfig config = Gecko::ServerConfig()
            .setPort(13514)
            .setThreadPoolSize(max_threads)
            .setIOThreadCount(20)
            .setMaxConnections(1000000)
            .setKeepAliveTimeout(30)
            .setMaxRequestBodySize(2 * 1024 * 1024)
            .enablePerformanceMonitoring();

        std::cout << "[LOG] Architecture highlights:" << std::endl;
        std::cout << "  [OK] Three-thread design (accept, IO, worker)" << std::endl;
        std::cout << "  [OK] Dedicated IO thread pool for network operations" << std::endl;
        std::cout << "  [OK] HTTP/1.1 keep-alive support" << std::endl;
        std::cout << "  [OK] Workers isolated from IO blocking" << std::endl;
        std::cout << "  [OK] High CPU utilization and concurrency" << std::endl;
        std::cout << "  [OK] Async IO pipeline" << std::endl;
        std::cout << "  [OK] Gin-style context API" << std::endl;
        std::cout << "  [OK] Onion-style middleware" << std::endl;
        std::cout << "  [OK] Fluent handler configuration" << std::endl;
        std::cout << "  [OK] Optional standalone logger subsystem" << std::endl;
        std::cout << "  [OK] Thread-safe logging" << std::endl;
        std::cout << "  [OK] Multiple output targets (console/file/both)" << std::endl;
        std::cout << "  [OK] Server decoupled from logging" << std::endl;
        std::cout << "\n[TIP] Usage notes:" << std::endl;
        std::cout << "  [FILE] Access log: access.log plus console output" << std::endl;
        std::cout << "  [FILE] Error log: error.log (hit /error-test?simulate=error to verify)" << std::endl;
        std::cout << "  [HOST]  Debug log: console only" << std::endl;
        std::cout << "[STATS] Detected " << max_threads << " hardware threads" << std::endl;
        std::cout << "[THREAD] Launching " << max_threads << " worker threads and 4 IO threads" << std::endl;
        std::cout << "[LOOP] Pipeline: accept (epoll) -> IO threads -> worker threads" << std::endl;
        std::cout << "\nPress Ctrl+C to stop the server\n" << std::endl;

        app.Run(config);

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
