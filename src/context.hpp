#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "http_request.hpp"
#include "http_response.hpp"
#include <any>
#include <functional>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>

namespace Gecko {

class Context {
public:
    Context(const HttpRequest &req)
    : request_(req), response_(HttpResponse::stockResponse(200)) {}
    const HttpRequest &request() const { return request_; }

    // get router params
    const std::string &param(const std::string &key) const;

    // get query params
    std::string query(const std::string &key) const;

    std::string header(const std::string &key) const;

    void set(const std::string &key, const std::any &value);

    template <typename T> T get(const std::string &key) const {
        std::shared_lock<std::shared_mutex> lock(context_data_mutex_);
        auto it = context_data_.find(key);
        if (it != context_data_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast &e) {
                throw std::runtime_error("Context data type mismatch for key: " + key);
            }
        }
        throw std::runtime_error("Context data not found for key: " + key);
    }

    bool has(const std::string &key) const;

    HttpResponse &response();

    Context &status(int code);

    void json(const nlohmann::json &data);

    void string(const std::string &data);

    void html(const std::string &html);

    Context &header(const std::string &key, const std::string &value);

    void setParams(const std::map<std::string, std::string> &params);

private:
    const HttpRequest &request_;
    HttpResponse response_;
    std::map<std::string, std::string> router_params_;
    mutable std::shared_mutex context_data_mutex_;
    std::map<std::string, std::any> context_data_;
};

using HandlerFunc = std::function<void(Context &)>;

} // namespace Gecko

#endif
