#ifndef ROUTER
#define ROUTER
#include "http_request.hpp"
#include "http_response.hpp"
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <optional>

namespace Gecko {

// 前向声明
class Context;

using RequestHandler = std::function<void(Context&)>;

struct Node {

    Node(std::string seg = "") : segment(std::move(seg)) {}

    std::string segment; // "user" ":id"
    std::map<std::string, std::unique_ptr<Node>> children;
    std::unique_ptr<Node> param_child = nullptr;
    std::string param_key;
    RequestHandler handler = nullptr;
};


//      /users/:id/posts -> ["users", ":id", "posts"]
inline auto split_path(const std::string &path) -> std::vector<std::string> {
    std::vector<std::string> result;
    std::stringstream ss(path);
    std::string segment;
    while (std::getline(ss, segment, '/')) {
        if (!segment.empty()) {
            result.push_back(segment);
        }
    }
    return result;
}

class Router{
public:
    void insert(Gecko::HttpMethod method, const std::string& path, RequestHandler handler);

    struct RouteMatchResult{
        RequestHandler handler;
        std::map<std::string, std::string> params;
    };

    auto find(Gecko::HttpMethod method,const std::string& path) const -> std::optional<RouteMatchResult>;

private:
    std::map<Gecko::HttpMethod, std::unique_ptr<Node>>roots_;
        
};

} // namespace Gecko

#endif
