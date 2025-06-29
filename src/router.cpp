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

} // namespace Gecko
