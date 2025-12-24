#include "rpc_server.hpp"

#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#ifdef GECKO_ENABLE_GRPC
#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/server_interceptor.h>

namespace Gecko {

class MiddlewareInterceptor : public grpc::experimental::Interceptor {
public:
    MiddlewareInterceptor(const std::vector<RpcMiddleware>& middlewares,
                          grpc::ServerContextBase* ctx)
        : middlewares_(middlewares), ctx_(ctx) {}

    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
        if (methods->QueryInterceptionHookPoint(
                grpc::experimental::InterceptionHookPoints::PRE_RECV_INITIAL_METADATA) &&
            ctx_) {
            auto* ctx = static_cast<grpc::ServerContext*>(ctx_);
            for (const auto& middleware : middlewares_) {
                grpc::Status status = middleware(*ctx);
                if (!status.ok()) {
                    denied_ = true;
                    failure_status_ = status;
                    ctx->TryCancel();
                    break;
                }
            }
        }

        if (denied_ &&
            methods->QueryInterceptionHookPoint(
                grpc::experimental::InterceptionHookPoints::PRE_SEND_STATUS)) {
            methods->ModifySendStatus(failure_status_);
        }

        methods->Proceed();
    }

private:
    const std::vector<RpcMiddleware>& middlewares_;
    grpc::ServerContextBase* ctx_{nullptr};
    bool denied_{false};
    grpc::Status failure_status_{grpc::StatusCode::CANCELLED, "unauthorized"};
};

class MiddlewareInterceptorFactory
    : public grpc::experimental::ServerInterceptorFactoryInterface {
public:
    explicit MiddlewareInterceptorFactory(const std::vector<RpcMiddleware>& middlewares)
        : middlewares_(middlewares) {}

    grpc::experimental::Interceptor*
    CreateServerInterceptor(grpc::experimental::ServerRpcInfo* info) override {
        return new MiddlewareInterceptor(middlewares_, info ? info->server_context() : nullptr);
    }

private:
    const std::vector<RpcMiddleware>& middlewares_;
};

class RpcServer::Impl {
public:
    explicit Impl(RpcConfig config) : config_(std::move(config)) {}

    void setConfig(const RpcConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (server_) {
            throw std::runtime_error("Cannot update RpcConfig while gRPC server is running");
        }
        config_ = config;
    }

    void AddService(grpc::Service* service) {
        if (!service) {
            throw std::runtime_error("RpcServer::AddService received null service");
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (server_) {
            throw std::runtime_error("Cannot register service after gRPC server has started");
        }
        services_.push_back(service);
    }

    void AddMiddleware(RpcMiddleware middleware) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (server_) {
            throw std::runtime_error("Cannot register middleware after gRPC server has started");
        }
        middlewares_.push_back(std::move(middleware));
    }

    void ClearMiddlewares() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (server_) {
            throw std::runtime_error("Cannot clear middleware after gRPC server has started");
        }
        middlewares_.clear();
    }

    void Start(bool spawn_waiter) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (server_) {
            throw std::runtime_error("gRPC server already started");
        }

        grpc::ServerBuilder builder;
        if (config_.enable_reflection) {
            grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        }
        builder.AddListeningPort(config_.address, grpc::InsecureServerCredentials());
        for (auto* service : services_) {
            builder.RegisterService(service);
        }

        if (!middlewares_.empty()) {
            std::vector<std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>> factories;
            factories.emplace_back(new MiddlewareInterceptorFactory(middlewares_));
            builder.experimental().SetInterceptorCreators(std::move(factories));
        }

        if (config_.max_send_message_size > 0) {
            builder.SetMaxSendMessageSize(config_.max_send_message_size);
        }
        if (config_.max_receive_message_size > 0) {
            builder.SetMaxReceiveMessageSize(config_.max_receive_message_size);
        }
        if (config_.num_completion_queues > 0) {
            builder.SetSyncServerOption(grpc::ServerBuilder::NUM_CQS, config_.num_completion_queues);
        }
        if (config_.min_pollers > 0) {
            builder.SetSyncServerOption(grpc::ServerBuilder::MIN_POLLERS, config_.min_pollers);
        }
        if (config_.max_pollers > 0) {
            builder.SetSyncServerOption(grpc::ServerBuilder::MAX_POLLERS, config_.max_pollers);
        }

        grpc::EnableDefaultHealthCheckService(config_.enable_health_check);

        server_ = builder.BuildAndStart();
        if (!server_) {
            throw std::runtime_error("Failed to start gRPC server on " + config_.address);
        }
        running_ = true;

        if (spawn_waiter) {
            wait_thread_ = std::thread([this]() {
                server_->Wait();
                std::lock_guard<std::mutex> lock(mutex_);
                running_ = false;
                server_.reset();
            });
        }
    }

    void Wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!server_) {
            return;
        }

        if (wait_thread_.joinable()) {
            auto waiter = std::move(wait_thread_);
            lock.unlock();
            waiter.join();
            lock.lock();
        } else {
            lock.unlock();
            server_->Wait();
            lock.lock();
        }
        running_ = false;
        server_.reset();
    }

    void Shutdown() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!server_) {
            return;
        }

        server_->Shutdown();
        lock.unlock();

        if (wait_thread_.joinable()) {
            wait_thread_.join();
        } else {
            server_->Wait();
        }

        lock.lock();
        running_ = false;
        server_.reset();
    }

    bool running() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

private:
    RpcConfig config_;
    std::vector<grpc::Service*> services_;
    std::vector<RpcMiddleware> middlewares_;
    std::unique_ptr<grpc::Server> server_;
    std::thread wait_thread_;
    mutable std::mutex mutex_;
    bool running_{false};
};

} // namespace Gecko

#else

namespace Gecko {

class RpcServer::Impl {
public:
    explicit Impl(RpcConfig config) : config_(std::move(config)) {}

    void setConfig(const RpcConfig& config) {
        config_ = config;
    }

    void AddService(grpc::Service*) {
        throw std::runtime_error("Gecko was built without GECKO_ENABLE_GRPC; rebuild with -DGECKO_ENABLE_GRPC=ON to use RpcServer");
    }

    void Start(bool) {
        throw std::runtime_error("Gecko was built without GECKO_ENABLE_GRPC; rebuild with -DGECKO_ENABLE_GRPC=ON to start RpcServer");
    }

    void Wait() {}

    void Shutdown() {}

    bool running() const { return false; }

private:
    RpcConfig config_;
};

} // namespace Gecko

#endif

namespace Gecko {

RpcServer::RpcServer(RpcConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

RpcServer::~RpcServer() {
    Shutdown();
}

RpcServer& RpcServer::setConfig(const RpcConfig& config) {
    impl_->setConfig(config);
    return *this;
}

RpcServer& RpcServer::AddService(grpc::Service* service) {
    impl_->AddService(service);
    return *this;
}

#ifdef GECKO_ENABLE_GRPC
RpcServer& RpcServer::AddMiddleware(RpcMiddleware middleware) {
    impl_->AddMiddleware(std::move(middleware));
    return *this;
}

RpcServer& RpcServer::ClearMiddlewares() {
    impl_->ClearMiddlewares();
    return *this;
}
#endif

void RpcServer::Start() {
    impl_->Start(false);
}

void RpcServer::StartBackground() {
    impl_->Start(true);
}

void RpcServer::Wait() {
    impl_->Wait();
}

void RpcServer::Shutdown() {
    impl_->Shutdown();
}

bool RpcServer::running() const {
    return impl_->running();
}

} // namespace Gecko
