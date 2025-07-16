#include "engine.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Gecko {

auto Engine::Static(const std::string &relativePath,
                    const std::string &root) -> Engine & {
    // TODO: 实现静态文件服务
    return *this;
}

void Engine::Run(const ServerConfig &config) {
    printServerInfo(config);
    Server server(config);
    server.run([this](Context &ctx) -> void { this->handleRequest(ctx); });
}

void Engine::handleRequest(Context &ctx) {
    auto result = router_.find(ctx.request().getMethod(), ctx.request().getUrl());
    if (!result.has_value()) {
        ctx.status(404).string("404 Not Found");
        return;
    }
    ctx.setParams(result->params);
    HandlerFunc finalHandler = result->handler;
    executeMiddlewares(ctx, finalHandler);
}

void Engine::executeMiddlewares(Context &ctx, HandlerFunc finalHandler) {
    if (middlewares_.empty()) {
        finalHandler(ctx);
        return;
    }

    std::function<void(int)> execute = [&](int index) -> void {
        if (index >= static_cast<int>(middlewares_.size())) {
            finalHandler(ctx);
            return;
        }
        std::function<void()> next = [&, index]() { execute(index + 1); };

        middlewares_[index](ctx, next);
    };

    execute(0);
}

void Engine::printServerInfo(const ServerConfig &config) {
    std::cout << "🦎 Gecko Web Framework" << std::endl;
    std::cout << "======================" << std::endl;
    std::cout << std::endl;

    // 基本配置信息
    std::cout << " 服务器配置:" << std::endl;
    std::cout << " 端口:           " << config.port << std::endl;
    std::cout << " 监听地址:       " << config.host << std::endl;
    std::cout << " 工作线程数:     " << config.thread_pool_size << std::endl;
    std::cout << " 最大连接数:     " << config.max_connections << std::endl;
    std::cout << " 请求体大小限制: " << (config.max_request_body_size / 1024) << " KB" << std::endl;
    std::cout << " Keep-Alive超时: " << config.keep_alive_timeout << 
        "秒" << std::endl;
        std::cout << std::endl;

    // 启动提示
    std::cout << "服务器启动中..." << std::endl;
    std::cout << "访问地址: http://" << (config.host == "0.0.0.0" ? "localhost" : config.host) << ":"
        << config.port << std::endl;
    std::cout << std::endl;

    std::cout << std::endl;

    std::cout << "按 Ctrl+C 停止服务器" << std::endl;
    std::cout << "===================" << std::endl;
}
} // namespace Gecko
