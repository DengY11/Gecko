#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "context.hpp"
#include "router.hpp"
#include "server.hpp"
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

    // === middleware support ===
    Engine& Use(MiddlewareFunc middleware) {
        middlewares_.push_back(middleware);
        return *this;
    }

    // === static file service ===
    Engine& Static(const std::string& relativePath, const std::string& root); 

    void Run(int port );
    void Run(const std::string& addr); 

    

private:
    Router router_;
    std::vector<MiddlewareFunc> middlewares_;

    void handleRequest(Context& ctx); 

    void executeMiddlewares(Context& ctx, HandlerFunc finalHandler); 
    
};

} // namespace Gecko

#endif 
