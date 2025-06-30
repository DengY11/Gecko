#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "http_request.hpp"
#include "http_response.hpp"
#include <string>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>

namespace Gecko {

class Context {
public:
    Context(const HttpRequest& req) : request_(req), response_(HttpResponse::stockResponse(200)) {}

    // === 请求相关 ===
    const HttpRequest& request() const { return request_; }
    
    // 获取路由参数 /users/:id -> params["id"]
    const std::string& param(const std::string& key) const {
        static const std::string empty;
        auto it = params_.find(key);
        return it != params_.end() ? it->second : empty;
    }
    
    // 获取查询参数 ?name=value
    std::string query(const std::string& key) const {
        // TODO: 解析URL查询参数
        return "";
    }
    
    // 获取请求头
    std::string header(const std::string& key) const {
        auto headers = request_.getHeaders();
        auto it = headers.find(key);
        return it != headers.end() ? it->second : "";
    }

    // === 响应相关 ===
    HttpResponse& response() { return response_; }
    
    // 设置状态码
    Context& status(int code) {
        response_.setStatusCode(code);
        return *this;
    }
    
    // JSON响应 - Gin风格
    void json(const nlohmann::json& data) {
        response_.addHeader("Content-Type", "application/json");
        response_.setBody(data.dump());
    }
    
    // 字符串响应
    void string(const std::string& data) {
        response_.addHeader("Content-Type", "text/plain; charset=utf-8");
        response_.setBody(data);
    }
    
    // HTML响应
    void html(const std::string& html) {
        response_.addHeader("Content-Type", "text/html; charset=utf-8");
        response_.setBody(html);
    }
    
    // 设置响应头
    Context& header(const std::string& key, const std::string& value) {
        response_.addHeader(key, value);
        return *this;
    }

    // === 内部方法（框架使用） ===
    void setParams(const std::map<std::string, std::string>& params) {
        params_ = params;
    }

private:
    const HttpRequest& request_;
    HttpResponse response_;
    std::map<std::string, std::string> params_;  // 路由参数
};

// Gin风格的Handler函数签名
using HandlerFunc = std::function<void(Context&)>;

} // namespace Gecko

#endif