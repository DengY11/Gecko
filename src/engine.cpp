#include "engine.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Gecko {

auto Engine::Static(const std::string &relativePath,
                    const std::string &root) -> Engine & {
    /* TODO: implement static file service */
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
    std::cout << "Gecko Web Framework" << std::endl;
    std::cout << "====================" << std::endl;
    std::cout << std::endl;

    /* Basic configuration output */
    std::cout << " Server configuration:" << std::endl;
    std::cout << " Port:              " << config.port << std::endl;
    std::cout << " Listen address:    " << config.host << std::endl;
    std::cout << " Worker threads:    " << config.thread_pool_size << std::endl;
    std::cout << " Max connections:   " << config.max_connections << std::endl;
    std::cout << " Body size limit:   " << (config.max_request_body_size / 1024) << " KB" << std::endl;
    std::cout << " Keep-Alive timeout:" << config.keep_alive_timeout << " seconds" << std::endl;
    std::cout << std::endl;

    /* Startup hints */
    std::cout << "Server starting..." << std::endl;
    std::cout << "Visit http://" << (config.host == "0.0.0.0" ? "localhost" : config.host) << ":"
        << config.port << std::endl;
    std::cout << std::endl;

    std::cout << std::endl;

    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    std::cout << "===================" << std::endl;
}
} /* namespace Gecko */
