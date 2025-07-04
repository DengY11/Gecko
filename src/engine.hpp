#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "context.hpp"
#include "router.hpp"
#include "server.hpp"
#include <vector>
#include <functional>

namespace Gecko {

// 中间件函数类型
using MiddlewareFunc = std::function<void(Context&, std::function<void()>)>;

class Engine {
public:
    Engine() = default;

    // === Gin风格路由注册 ===
    Engine& GET(const std::string& path, HandlerFunc handler) {
        router_.insert(HttpMethod::GET, path, wrapHandler(handler));
        return *this;
    }

    Engine& POST(const std::string& path, HandlerFunc handler) {
        router_.insert(HttpMethod::POST, path, wrapHandler(handler));
        return *this;
    }

    Engine& PUT(const std::string& path, HandlerFunc handler) {
        router_.insert(HttpMethod::PUT, path, wrapHandler(handler));
        return *this;
    }

    Engine& DELETE(const std::string& path, HandlerFunc handler) {
        router_.insert(HttpMethod::DELETE, path, wrapHandler(handler));
        return *this;
    }

    Engine& HEAD(const std::string& path, HandlerFunc handler) {
        router_.insert(HttpMethod::HEAD, path, wrapHandler(handler));
        return *this;
    }

    // === 中间件支持 ===
    Engine& Use(MiddlewareFunc middleware) {
        middlewares_.push_back(middleware);
        return *this;
    }

    // === 静态文件服务 ===
    Engine& Static(const std::string& relativePath, const std::string& root) {
        // TODO: 实现静态文件服务
        return *this;
    }

    // === 启动服务器 ===
    void Run(int port = 8080) {
        Server server(port);
        
        // 将Engine的路由处理逻辑传递给底层Server
        server.run([this](const HttpRequest& req) -> HttpResponse {
            return this->handleRequest(req);
        });
    }

    void Run(const std::string& addr) {
        // 解析地址格式 "localhost:8080"
        size_t pos = addr.find(':');
        if (pos != std::string::npos) {
            int port = std::stoi(addr.substr(pos + 1));
            Run(port);
        } else {
            Run(8080);  // 默认端口
        }
    }

private:
    Router router_;
    std::vector<MiddlewareFunc> middlewares_;

    // 包装HandlerFunc为旧的RequestHandler
    Server::RequestHandler wrapHandler(HandlerFunc handler) {
        return [handler, this](const HttpRequest& req) -> HttpResponse {
            Context ctx(req);
            
            // 执行中间件链
            executeMiddlewares(ctx, handler);
            
            return ctx.response();
        };
    }

    // 处理请求的核心逻辑
    HttpResponse handleRequest(const HttpRequest& req) {
        // 路由匹配
        auto result = router_.find(req.getMethod(), req.getUrl());
        
        if (!result.has_value()) {
            // 404 未找到
            auto response = HttpResponse::stockResponse(404);
            response.setBody("404 Not Found");
            return response;
        }

        // 创建Context并设置路由参数
        Context ctx(req);
        ctx.setParams(result->params);

        // 执行Handler（通过中间件链）
        HandlerFunc finalHandler = [&result](Context& ctx) {
            // 这里需要从Router的Handler转换为我们的HandlerFunc
            // 由于Router::RequestHandler签名不同，需要适配
            auto oldStyleHandler = result->handler;
            auto response = oldStyleHandler(ctx.request());
            ctx.response() = response;
        };

        executeMiddlewares(ctx, finalHandler);
        
        return ctx.response();
    }

    // 执行中间件链
    void executeMiddlewares(Context& ctx, HandlerFunc finalHandler) {
        if (middlewares_.empty()) {
            finalHandler(ctx);
            return;
        }

        // 构建中间件调用链
        std::function<void()> next;
        int index = -1;

        std::function<void()> dispatch = [&]() {
            index++;
            if (index < static_cast<int>(middlewares_.size())) {
                middlewares_[index](ctx, dispatch);
            } else if (index == static_cast<int>(middlewares_.size())) {
                finalHandler(ctx);
            }
        };

        dispatch();
    }
};

} // namespace Gecko

#endif 
