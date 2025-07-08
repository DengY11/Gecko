#include "src/engine.hpp"
#include <iostream>
#include <chrono>

int main() {
    try {
        std::cout << "🦎 Gecko Web Framework - Gin风格API示例" << std::endl;
        std::cout << "==========================================" << std::endl;

        // 创建Engine实例
        Gecko::Engine app;

        // 使用中间件记录请求
        app.Use([](Gecko::Context& ctx, std::function<void()> next) {
            auto start = std::chrono::high_resolution_clock::now();
            std::cout << "[中间件] 请求开始: " << ctx.request().getUrl() << std::endl;
            
            next(); // 调用下一个中间件或处理器
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            std::cout << "[中间件] 请求完成: " << duration.count() << "μs" << std::endl;
        });

        // 添加CORS中间件
        app.Use([](Gecko::Context& ctx, std::function<void()> next) {
            ctx.header("Access-Control-Allow-Origin", "*")
               .header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE")
               .header("Access-Control-Allow-Headers", "Content-Type");
            next();
        });

        // 路由定义 - Gin风格
        app.GET("/", [](Gecko::Context& ctx) {
            ctx.html(R"(<!DOCTYPE html>
<html>
<head>
    <title>🦎 Gecko Web Framework</title>
    <meta charset="utf-8">
    <style>
        body { font-family: 'Segoe UI', sans-serif; margin: 40px; background: #f5f5f5; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #2e7d32; text-align: center; }
        .endpoint { margin: 15px 0; padding: 15px; background: #f8f9fa; border-left: 4px solid #2e7d32; border-radius: 5px; }
        a { color: #1976d2; text-decoration: none; font-weight: 500; }
        a:hover { text-decoration: underline; }
        .badge { background: #2e7d32; color: white; padding: 2px 6px; border-radius: 3px; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🦎 Gecko Web Framework</h1>
        <p><strong>现在支持Gin风格的API！</strong></p>
        
        <h2>🎯 API端点：</h2>
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
            <span class="badge">GET</span> <a href="/headers">/headers</a> - 请求头信息
        </div>
        <div class="endpoint">
            <span class="badge">GET</span> <a href="/api/users/123">/api/users/:id</a> - RESTful API示例
        </div>
        
        <h2>✨ 新特性：</h2>
        <ul>
            <li>🎯 <strong>Gin风格API</strong> - Context作为唯一参数</li>
            <li>🧅 <strong>洋葱模型中间件</strong> - 支持前置和后置逻辑</li>
            <li>📝 <strong>简洁的响应方法</strong> - json(), html(), string()</li>
            <li>🔗 <strong>链式调用</strong> - status().header().json()</li>
            <li>⚡ <strong>高性能路由</strong> - 基数树实现</li>
        </ul>
    </div>
</body>
</html>)");
        });

        app.GET("/ping", [](Gecko::Context& ctx) {
            nlohmann::json response = {
                {"message", "pong"},
                {"framework", "Gecko Web Framework"},
                {"status", "running"},
                {"style", "gin-like"},
                {"features", nlohmann::json::array({
                    "context-based handlers",
                    "onion-model middleware", 
                    "method chaining",
                    "path parameters",
                    "query parameters"
                })}
            };
            ctx.json(response);
        });

        app.GET("/hello/:name", [](Gecko::Context& ctx) {
            std::string name = ctx.param("name");
            if (name.empty()) {
                ctx.status(400).json({{"error", "name parameter is required"}});
                return;
            }
            
            nlohmann::json response = {
                {"message", "Hello, " + name + "!"},
                {"path_param", name},
                {"framework", "Gecko"}
            };
            ctx.status(200).json(response);
        });

        app.GET("/search", [](Gecko::Context& ctx) {
            std::string query = ctx.query("q");
            std::string type = ctx.query("type");
            
            nlohmann::json response = {
                {"search_query", query},
                {"search_type", type},
                {"results", nlohmann::json::array()},
                {"total", 0}
            };
            
            if (!query.empty()) {
                response["results"] = nlohmann::json::array({
                    {{"id", 1}, {"title", "Gecko Web Framework"}, {"description", "A fast C++ web framework"}},
                    {{"id", 2}, {"title", "Gecko Router"}, {"description", "High-performance HTTP router"}}
                });
                response["total"] = 2;
            }
            
            ctx.json(response);
        });

        app.GET("/headers", [](Gecko::Context& ctx) {
            auto headers = ctx.request().getHeaders();
            nlohmann::json response = {
                {"headers", nlohmann::json::object()},
                {"count", headers.size()},
                {"user_agent", ctx.header("User-Agent")},
                {"host", ctx.header("Host")}
            };
            
            for (const auto& header : headers) {
                response["headers"][header.first] = header.second;
            }
            
            ctx.json(response);
        });

        app.GET("/api/users/:id", [](Gecko::Context& ctx) {
            std::string userId = ctx.param("id");
            
            // 模拟数据库查询
            if (userId == "123") {
                nlohmann::json user = {
                    {"id", 123},
                    {"name", "张三"},
                    {"email", "zhangsan@example.com"},
                    {"role", "admin"},
                    {"created_at", "2024-01-25T12:00:00Z"}
                };
                ctx.json(user);
            } else if (userId == "456") {
                nlohmann::json user = {
                    {"id", 456},
                    {"name", "李四"},
                    {"email", "lisi@example.com"},
                    {"role", "user"},
                    {"created_at", "2024-01-20T08:30:00Z"}
                };
                ctx.json(user);
            } else {
                ctx.status(404).json({
                    {"error", "User not found"},
                    {"user_id", userId}
                });
            }
        });

        // POST示例
        app.POST("/api/users", [](Gecko::Context& ctx) {
            // 在实际应用中，这里会解析请求体的JSON数据
            nlohmann::json response = {
                {"message", "User created successfully"},
                {"id", 789},
                {"method", "POST"},
                {"note", "This is a demo endpoint"}
            };
            ctx.status(201).json(response);
        });

        // 启动服务器
        std::cout << "\n🚀 启动服务器..." << std::endl;
        std::cout << "端口: 13514" << std::endl;
        std::cout << "访问: http://localhost:8080" << std::endl;
        std::cout << "\n📝 特性展示:" << std::endl;
        std::cout << "  ✅ Gin风格的Context API" << std::endl;
        std::cout << "  ✅ 洋葱模型中间件" << std::endl;
        std::cout << "  ✅ 链式方法调用" << std::endl;
        std::cout << "  ✅ 路径参数和查询参数" << std::endl;
        std::cout << "  ✅ JSON/HTML/Text响应" << std::endl;
        std::cout << "\n按 Ctrl+C 停止服务器\n" << std::endl;

        app.Run(13514);

    } catch (const std::exception& e) {
        std::cerr << "❌ 错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 