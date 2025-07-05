#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "http_request.hpp"
#include "http_response.hpp"
#include <mutex>
#include <shared_mutex>
#include <string>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>
#include <any>

namespace Gecko {

class Context {
public:
    Context(const HttpRequest& req) : request_(req), response_(HttpResponse::stockResponse(200)) {}
    const HttpRequest& request() const { return request_; }
    // get router params
    const std::string& param(const std::string& key) const ;
    // get query params
    std::string query(const std::string& key) const ;
    std::string header(const std::string& key) const {
        auto headers = request_.getHeaders();
        auto it = headers.find(key);
        return it != headers.end() ? it->second : "";
    }

    void set(const std::string& key, const std::any& value) {
        std::lock_guard<std::shared_mutex> lock(context_data_mutex_);
        context_data_[key] = value;
    }
    template<typename T>
    T get(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(context_data_mutex_);
        auto it = context_data_.find(key);
        if (it != context_data_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast& e) {
                throw std::runtime_error("Context data type mismatch for key: " + key);
            }
        }
        throw std::runtime_error("Context data not found for key: " + key);
    }
    bool has(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(context_data_mutex_);
        return context_data_.find(key) != context_data_.end();
    }

    HttpResponse& response() { return response_; }
    Context& status(int code) {
        response_.setStatusCode(code);
        return *this;
    }
    void json(const nlohmann::json& data) {
        response_.addHeader("Content-Type", "application/json");
        response_.setBody(data.dump());
    }
    void string(const std::string& data) {
        response_.addHeader("Content-Type", "text/plain; charset=utf-8");
        response_.setBody(data);
    }
    void html(const std::string& html) {
        response_.addHeader("Content-Type", "text/html; charset=utf-8");
        response_.setBody(html);
    }
    Context& header(const std::string& key, const std::string& value) {
        response_.addHeader(key, value);
        return *this;
    }

    void setParams(const std::map<std::string, std::string>& params) ;

private:
    const HttpRequest& request_;
    HttpResponse response_;
    std::map<std::string, std::string> router_params_;  // 路由参数
    
    mutable std::shared_mutex context_data_mutex_;
    std::map<std::string, std::any> context_data_;  // 上下文传递的kv对
};

// Gin风格的Handler函数签名
using HandlerFunc = std::function<void(Context&)>;

} // namespace Gecko

#endif
