#include "router.hpp"

namespace Gecko {

void Router::insert(Gecko::HttpMethod method, const std::string &path,
                    RequestHandler handler) {
    if (roots_.find(method) == roots_.end()) {
        roots_[method] = std::make_unique<Node>();
    }
    Node *current = roots_.at(method).get();
    auto segments = split_path(path);
    for (const auto &seg : segments) {
        if (seg.empty()) {
            continue;
        }
        if (seg[0] == ':') {
            // this is a param segment like ":id"
            if (!current->param_child) {
                current->param_child = std::make_unique<Node>(seg);
                current->param_key = seg.substr(1);
            }
            current = current->param_child.get();
        } else {
            // this is a static segment
            if (current->children.find(seg) == current->children.end()) {
                current->children[seg] = std::make_unique<Node>(seg);
            }
            current = current->children.at(seg).get();
        }
    }
    current->handler = handler;
}

auto Router::find(Gecko::HttpMethod method, const std::string &path) const
-> std::optional<RouteMatchResult> {
    auto method_iter = roots_.find(method);
    if (method_iter == roots_.end()) {
        return std::nullopt;
    }
    const Node *current_iter = method_iter->second.get();
    auto segments = split_path(path);
    RouteMatchResult ret{};
    for (const auto &seg : segments) {
        auto child_iter = current_iter->children.find(seg);
        if (child_iter != current_iter->children.end()) {
            current_iter = child_iter->second.get();
        } else if (current_iter->param_child) {
            ret.params[current_iter->param_key] = seg;
            current_iter = current_iter->param_child.get();
        } else {
            return std::nullopt;
        }
    }
    if (current_iter && current_iter->handler) {
        ret.handler = current_iter->handler;
        return ret;
    }
    return std::nullopt;
}
} // namespace Gecko
