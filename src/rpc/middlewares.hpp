#ifndef GECKO_RPC_MIDDLEWARES_HPP
#define GECKO_RPC_MIDDLEWARES_HPP

#include "rpc_server.hpp"
#include <stdexcept>
#include <string>

#ifdef GECKO_ENABLE_GRPC
#include <grpcpp/server_context.h>
#endif

namespace Gecko {
namespace RpcMiddlewares {

#ifdef GECKO_ENABLE_GRPC
inline RpcMiddleware RequireMetadata(const std::string& key, const std::string& expected_value) {
    return [key, expected_value](grpc::ServerContext& ctx) -> grpc::Status {
        auto& md = ctx.client_metadata();
        auto range = md.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            std::string value(it->second.data(), it->second.length());
            if (value == expected_value) {
                return grpc::Status::OK;
            }
        }
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Missing or invalid metadata: " + key);
    };
}

inline RpcMiddleware RequireJwtToken(const std::string& expected_token,
                                     const std::string& scheme = "Bearer") {
    return [expected_token, scheme](grpc::ServerContext& ctx) -> grpc::Status {
        auto& md = ctx.client_metadata();
        auto it = md.find("authorization");
        if (it == md.end()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authorization header missing");
        }
        std::string auth_value(it->second.data(), it->second.length());
        std::string prefix = scheme + " ";
        if (auth_value.rfind(prefix, 0) != 0) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authorization scheme mismatch");
        }
        std::string token = auth_value.substr(prefix.size());
        if (token != expected_token) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid token");
        }
        return grpc::Status::OK;
    };
}
#else
inline RpcMiddleware RequireMetadata(const std::string&, const std::string&) {
    throw std::runtime_error("Rpc middleware requires gRPC; build with -DGECKO_ENABLE_GRPC=ON");
}

inline RpcMiddleware RequireJwtToken(const std::string&, const std::string& = "Bearer") {
    throw std::runtime_error("Rpc middleware requires gRPC; build with -DGECKO_ENABLE_GRPC=ON");
}
#endif

} // namespace RpcMiddlewares
} // namespace Gecko

#endif
