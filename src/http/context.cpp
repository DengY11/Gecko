#include "context.hpp"
#include "shared_mutex"

namespace Gecko {

auto Context::param(const std::string &key) const -> const std::string & {
    static const std::string empty;
    auto it = router_params_.find(key);
    return it != router_params_.end() ? it->second : empty;
}

void Context::setParams(const std::map<std::string, std::string> &params) {
    router_params_ = params;
}

std::string Context::query(const std::string &key) const {
    return request_.getQueryParam(key);
}

std::string Context::header(const std::string& key) const {
    auto headers = request_.getHeaders();
    auto it = headers.find(key);
    return it != headers.end() ? it->second : "";
}

void Context::set(const std::string& key, const std::any& value) {
    std::lock_guard<std::shared_mutex> lock(context_data_mutex_);
    context_data_[key] = value;
}

bool Context::has(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(context_data_mutex_);
    return context_data_.find(key) != context_data_.end();
}

HttpResponse& Context::response() {
    return response_;
}

Context& Context::status(int code) {
    response_.setStatusCode(code);
    return *this;
}

void Context::json(const nlohmann::json& data) {
    response_.addHeader("Content-Type", "application/json");
    response_.setBody(data.dump());
}

void Context::string(const std::string& data) {
    response_.addHeader("Content-Type", "text/plain; charset=utf-8");
    response_.setBody(data);
}

void Context::html(const std::string& html) {
    response_.addHeader("Content-Type", "text/html; charset=utf-8");
    response_.setBody(html);
}

Context& Context::header(const std::string& key, const std::string& value) {
    response_.addHeader(key, value);
    return *this;
}

} // namespace Gecko
