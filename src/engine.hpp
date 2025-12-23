#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "context.hpp"
#include "router.hpp"
#include "server.hpp"
#include "server_config.hpp"
#include <vector>
#include <functional>

namespace Gecko {

using MiddlewareFunc = std::function<void(Context&, std::function<void()>)>;

class Engine {
public:
    Engine() = default;

    Engine& GET(const std::string& path, HandlerFunc handler) {
        router_.insert(HttpMethod::GET, path, handler);
        return *this;
    }

    Engine& POST(const std::string& path, HandlerFunc handler) {
        router_.insert(HttpMethod::POST, path, handler);
        return *this;
    }

    Engine& PUT(const std::string& path, HandlerFunc handler) {
        router_.insert(HttpMethod::PUT, path, handler);
        return *this;
    }

    Engine& DELETE(const std::string& path, HandlerFunc handler) {
        router_.insert(HttpMethod::DELETE, path, handler);
        return *this;
    }

    Engine& HEAD(const std::string& path, HandlerFunc handler) {
        router_.insert(HttpMethod::HEAD, path, handler);
        return *this;
    }

    /* Middleware support */
    Engine& Use(MiddlewareFunc middleware) {
        middlewares_.push_back(middleware);
        return *this;
    }

    /* Static file service */
    Engine& Static(const std::string& relativePath, const std::string& root); 

    void Run(const ServerConfig& config);
    void Run(int port = 8080) {
        ServerConfig config(port);
        Run(config);
    }
    void Run(const std::string& addr) {
        size_t pos = addr.find(':');
        int port = 8080;
        if (pos != std::string::npos) {
            port = std::stoi(addr.substr(pos + 1));
        }
        Run(port);
    }
    void RunWithThreads(int port, size_t thread_count) {
        ServerConfig config(port, thread_count);
        Run(config);
    }

private:
    Router router_;
    std::vector<MiddlewareFunc> middlewares_;

    void handleRequest(Context& ctx); 
    void executeMiddlewares(Context& ctx, HandlerFunc finalHandler); 
    void printServerInfo(const ServerConfig& config); 
};

} // namespace Gecko

#endif 
