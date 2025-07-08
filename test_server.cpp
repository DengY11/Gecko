#include "src/server.hpp"
#include "src/context.hpp"
#include <iostream>

int main() {
    try {
        std::cout << "🦎 测试 Gecko Web Framework Server 类" << std::endl;
        std::cout << "====================================" << std::endl;

        // 创建请求处理函数（使用Gin风格的Context）
        auto handler = [](Gecko::Context& ctx) -> void {
            const auto& request = ctx.request();
            std::cout << "收到请求: " << Gecko::HttpMethodToString(request.getMethod()) 
                      << " " << request.getUrl() << std::endl;
            
            // 打印请求头信息
            auto headers = request.getHeaders();
            std::cout << "请求头数量: " << headers.size() << std::endl;
            for (const auto& header : headers) {
                std::cout << "  " << header.first << ": " << header.second << std::endl;
            }
            
            // 简单路由处理
            std::string url = request.getUrl();
            if (url == "/") {
                ctx.html(R"(<!DOCTYPE html>
<html>
<head>
    <title>🦎 Gecko Server Test</title>
    <meta charset="utf-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        h1 { color: #2e7d32; }
        .endpoint { margin: 10px 0; padding: 10px; background: #f5f5f5; border-radius: 5px; }
        a { color: #1976d2; text-decoration: none; }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <h1>🦎 Gecko Web Framework - Server 测试</h1>
    <p>恭喜！Server类工作正常！</p>
    
    <h2>🧪 测试端点：</h2>
    <div class="endpoint"><a href="/ping">🏓 /ping - JSON测试</a></div>
    <div class="endpoint"><a href="/hello">👋 /hello - 简单文本</a></div>
    <div class="endpoint"><a href="/headers">📋 /headers - 显示请求头</a></div>
    <div class="endpoint"><a href="/status/404">❌ /status/404 - 测试404错误</a></div>
    <div class="endpoint"><a href="/status/500">💥 /status/500 - 测试500错误</a></div>
    
    <h2>📊 服务器信息：</h2>
    <ul>
        <li><strong>Framework:</strong> Gecko Web Framework</li>
        <li><strong>Language:</strong> C++17</li>
        <li><strong>Network:</strong> Epoll + Non-blocking IO</li>
        <li><strong>HTTP:</strong> HTTP/1.1</li>
    </ul>
</body>
</html>)");
                
            } else if (url == "/ping") {
                nlohmann::json jsonResponse = {
                    {"message", "pong"},
                    {"framework", "Gecko Web Framework"},
                    {"status", "running"},
                    {"method", Gecko::HttpMethodToString(request.getMethod())},
                    {"timestamp", "2024-01-25T12:00:00Z"}
                };
                ctx.json(jsonResponse);
                
            } else if (url == "/hello") {
                ctx.string("🦎 你好！这是来自 Gecko Web Framework 的问候！\n\n服务器正在正常运行。");
                
            } else if (url == "/headers") {
                std::string body = R"(<!DOCTYPE html>
<html>
<head>
    <title>请求头信息</title>
    <meta charset="utf-8">
    <style>body{font-family:Arial;margin:40px;} table{border-collapse:collapse;width:100%;} th,td{border:1px solid #ddd;padding:8px;text-align:left;} th{background:#f2f2f2;}</style>
</head>
<body>
    <h1>📋 请求头信息</h1>
    <table>
        <tr><th>Header名称</th><th>值</th></tr>)";
                
                for (const auto& header : headers) {
                    body += "<tr><td>" + header.first + "</td><td>" + header.second + "</td></tr>";
                }
                body += R"(    </table>
    <p><a href="/">← 返回首页</a></p>
</body>
</html>)";
                ctx.html(body);
                
            } else if (url == "/status/404") {
                ctx.status(404).html(R"(<!DOCTYPE html>
<html>
<head><title>404 测试</title></head>
<body>
    <h1>❌ 404 - 这是故意的测试错误</h1>
    <p>这个端点故意返回404状态码来测试错误处理。</p>
    <p><a href="/">← 返回首页</a></p>
</body>
</html>)");
                
            } else if (url == "/status/500") {
                ctx.status(500).html(R"(<!DOCTYPE html>
<html>
<head><title>500 测试</title></head>
<body>
    <h1>💥 500 - 这是故意的测试错误</h1>
    <p>这个端点故意返回500状态码来测试服务器错误处理。</p>
    <p><a href="/">← 返回首页</a></p>
</body>
</html>)");
                
            } else {
                // 真正的404
                ctx.status(404).html(R"(<!DOCTYPE html>
<html>
<head><title>404 Not Found</title></head>
<body>
    <h1>🚫 404 - 页面未找到</h1>
    <p>请求的路径 <strong>)" + url + R"(</strong> 不存在。</p>
    <p><a href="/">← 返回首页</a></p>
</body>
</html>)");
            }
        };

        std::cout << "\n🚀 启动测试服务器..." << std::endl;
        std::cout << "端口: 13513" << std::endl;
        std::cout << "测试地址: http://localhost:13514" << std::endl;
        std::cout << "\n📋 测试步骤:" << std::endl;
        std::cout << "1. 在浏览器中访问 http://localhost:13514" << std::endl;
        std::cout << "2. 点击各个测试链接验证功能" << std::endl;
        std::cout << "3. 观察终端输出的请求日志" << std::endl;
        std::cout << "4. 按 Ctrl+C 停止服务器\n" << std::endl;

        // 创建并启动服务器
        Gecko::Server server(13513);
        server.run(handler);

    } catch (const std::exception& e) {
        std::cerr << "❌ 服务器错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 