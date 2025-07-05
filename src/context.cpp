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





} // namespace Gecko
