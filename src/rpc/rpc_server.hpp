#ifndef GECKO_RPC_SERVER_HPP
#define GECKO_RPC_SERVER_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace grpc {
class Service;
class ServerContext;
class Status;
} // namespace grpc

namespace Gecko {

#ifdef GECKO_ENABLE_GRPC
using RpcMiddleware = std::function<::grpc::Status(::grpc::ServerContext&)>;
#endif

struct RpcConfig {
    std::string address = "0.0.0.0:50051";
    int max_send_message_size = 4 * 1024 * 1024;
    int max_receive_message_size = 4 * 1024 * 1024;
    int min_pollers = 0;
    int max_pollers = 0;
    int num_completion_queues = 0;
    bool enable_health_check = true;
    bool enable_reflection = true;

    RpcConfig& setAddress(const std::string& addr) {
        address = addr;
        return *this;
    }

    RpcConfig& setMaxMessageSize(int bytes) {
        max_send_message_size = bytes;
        max_receive_message_size = bytes;
        return *this;
    }

    RpcConfig& setThreadHints(int min_threads, int max_threads, int completion_queues = 0) {
        min_pollers = min_threads;
        max_pollers = max_threads;
        num_completion_queues = completion_queues;
        return *this;
    }

    RpcConfig& enableHealthCheck(bool enable = true) {
        enable_health_check = enable;
        return *this;
    }

    RpcConfig& enableReflection(bool enable = true) {
        enable_reflection = enable;
        return *this;
    }
};

class RpcServer {
public:
    explicit RpcServer(RpcConfig config = RpcConfig());
    ~RpcServer();

    RpcServer(const RpcServer&) = delete;
    RpcServer& operator=(const RpcServer&) = delete;
    RpcServer(RpcServer&&) noexcept = default;
    RpcServer& operator=(RpcServer&&) noexcept = default;

    RpcServer& setConfig(const RpcConfig& config);
    RpcServer& AddService(grpc::Service* service);
#ifdef GECKO_ENABLE_GRPC
    RpcServer& AddMiddleware(RpcMiddleware middleware);
    RpcServer& ClearMiddlewares();
#endif

    void Start();             /* Start without blocking current thread */
    void StartBackground();   /* Start and keep a waiter thread alive */
    void Wait();              /* Block until Shutdown is called */
    void Shutdown();          /* Graceful stop */

    bool running() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Gecko

#endif /* GECKO_RPC_SERVER_HPP */
