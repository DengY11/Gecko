#include "src/engine.hpp"
#include "src/logger.hpp"
#include <iostream>
#include <chrono>
#include <cmath>
#include <thread>

using namespace Gecko;

// CPU密集型计算函数
double cpu_intensive_calculation(int iterations) {
    double result = 0.0;
    for (int i = 0; i < iterations; ++i) {
        result += std::sin(i) * std::cos(i) * std::sqrt(i + 1);
        result = std::fmod(result, 1000000.0); // 防止结果过大
    }
    return result;
}

int main() {
    std::cout << "🦎 Gecko Web Framework - CPU密集型测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 创建Engine实例
    Engine app;
    
    // 简单ping接口（低CPU）
    app.GET("/ping", [](Context& ctx) {
        ctx.json({{"message", "pong"}, {"timestamp", std::time(nullptr)}});
    });
    
    // CPU密集型接口（高CPU消耗）
    app.GET("/cpu-test", [](Context& ctx) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 执行CPU密集型计算
        double result = cpu_intensive_calculation(1000000); // 100万次计算
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        
        ctx.json( {
            {"message", "Mixed load completed"},
            {"result", result},
            {"duration_us", duration.count()}
        });
    });
    
    // 混合负载接口
    app.GET("/mixed-load", [](Context& ctx) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 中等CPU负载
        double result = cpu_intensive_calculation(100000); // 10万次计算
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        ctx.json( {
            {"message", "Mixed load completed"},
            {"result", result},
            {"duration_us", duration.count()}
        });
    });
    
    // 服务器配置
    ServerConfig config(13515); // 使用不同端口避免冲突
    config.setThreadPoolSize(80)     // 80个工作线程
          .setIOThreadCount(8)       // 8个IO线程
          .setMaxConnections(20000)  // 增加连接数限制
          .setKeepAliveTimeout(60);
    
    std::cout << "🎯 测试接口:" << std::endl;
    std::cout << "   ├─ GET /ping        - 简单ping (低CPU)" << std::endl;
    std::cout << "   ├─ GET /cpu-test    - CPU密集型 (高CPU)" << std::endl;
    std::cout << "   └─ GET /mixed-load  - 混合负载 (中CPU)" << std::endl;
    std::cout << std::endl;
    
    std::cout << "💡 测试建议:" << std::endl;
    std::cout << "   🔸 测试 /ping:       k6 run --vus 1000 --duration 30s -e URL=http://localhost:13515/ping" << std::endl;
    std::cout << "   🔸 测试 /cpu-test:   k6 run --vus 100 --duration 30s -e URL=http://localhost:13515/cpu-test" << std::endl;
    std::cout << "   🔸 测试 /mixed-load: k6 run --vus 500 --duration 30s -e URL=http://localhost:13515/mixed-load" << std::endl;
    std::cout << std::endl;
    
    std::cout << "🚀 启动服务器..." << std::endl;
    
    // 启动服务器
    app.Run(config);
    
    return 0;
} 