#ifndef MIDDLEWARES_H
#define MIDDLEWARES_H
#include "context.hpp"

namespace Gecko {

class GeckoMiddleware {
public:
    static void CORS(Context &ctx, std::function<void()> next) {
        ctx.header("Access-Control-Allow-Origin", "*");
        ctx.header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
        ctx.header("Access-Control-Allow-Headers", "Content-Type");
        next();
    }
};

} // namespace Gecko

#endif
