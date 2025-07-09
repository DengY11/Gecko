#include "engine.hpp"

namespace Gecko {

auto Engine::Static(const std::string &relativePath, const std::string &root)
    -> Engine & {
  // TODO:
}

void Engine::Run(int port = 8080) {
  // 使用默认线程池大小（硬件并发数）
  Server server(port);
  server.run([this](Context &ctx) -> void {
    this->handleRequest(ctx);
  });
}

void Engine::Run(const std::string &addr) {
  size_t pos = addr.find(':');
  if (pos != std::string::npos) {
    int port = std::stoi(addr.substr(pos + 1));
    Run(port);
  } else {
    Run(8080);
  }
}

// 新增方法：支持自定义线程池大小
void Engine::RunWithThreads(int port, size_t thread_count) {
  Server server(port, thread_count);
  server.run([this](Context &ctx) -> void {
    this->handleRequest(ctx);
  });
}

void Engine::handleRequest(Context &ctx) {
  auto result = router_.find(ctx.request().getMethod(), ctx.request().getUrl());
  if (!result.has_value()) {
    ctx.status(404).string("404 Not Found");
    return;
  }

  ctx.setParams(result->params);

  HandlerFunc finalHandler = result->handler;

  executeMiddlewares(ctx, finalHandler);
}



void Engine::executeMiddlewares(Context &ctx, HandlerFunc finalHandler) {
  if (middlewares_.empty()) {
    finalHandler(ctx);
    return;
  }

  std::function<void(int)> execute = [&](int index) -> void {
    if (index >= static_cast<int>(middlewares_.size())) {
      finalHandler(ctx);
      return;
    }
    std::function<void()> next = [&, index]() {
      execute(index + 1); 
    };

    middlewares_[index](ctx, next);
  };

  execute(0);
}
} // namespace Gecko