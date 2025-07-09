#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <string>
#include <cstddef>
#include <thread>

namespace Gecko {

// 日志等级枚举
enum class LogLevel {
    DEBUG = 0,    // 调试信息
    INFO = 1,     // 一般信息  
    WARN = 2,     // 警告信息
    ERROR = 3,    // 错误信息
    OFF = 4       // 关闭日志
};

// 服务器配置结构体
struct ServerConfig {
    // 网络配置
    int port = 8080;                    // 监听端口
    std::string host = "0.0.0.0";      // 监听地址
    
    // 线程池配置
    size_t thread_pool_size = 0;       // 工作线程数，0表示使用硬件并发数
    
    // 日志配置
    LogLevel log_level = LogLevel::INFO; // 日志等级
    bool enable_access_log = true;       // 是否启用访问日志
    bool enable_error_log = true;        // 是否启用错误日志
    std::string log_format = "[{timestamp}] {level} {message}"; // 日志格式
    
    // 性能配置
    int max_connections = 10000;        // 最大连接数
    int keep_alive_timeout = 30;        // Keep-Alive超时时间（秒）
    size_t max_request_body_size = 1024 * 1024; // 最大请求体大小（1MB）
    
    // 默认构造函数
    ServerConfig() {
        // 默认线程池大小为硬件并发数
        if (thread_pool_size == 0) {
            thread_pool_size = std::thread::hardware_concurrency();
            if (thread_pool_size == 0) {
                thread_pool_size = 4; // 后备默认值
            }
        }
    }
    
    // 便捷构造函数
    explicit ServerConfig(int port) : ServerConfig() {
        this->port = port;
    }
    
    ServerConfig(int port, size_t threads) : ServerConfig(port) {
        this->thread_pool_size = threads;
    }
    
    // 链式配置方法
    ServerConfig& setPort(int port) {
        this->port = port;
        return *this;
    }
    
    ServerConfig& setHost(const std::string& host) {
        this->host = host;
        return *this;
    }
    
    ServerConfig& setThreadPoolSize(size_t size) {
        this->thread_pool_size = size;
        return *this;
    }
    
    ServerConfig& setLogLevel(LogLevel level) {
        this->log_level = level;
        return *this;
    }
    
    ServerConfig& enableAccessLog(bool enable = true) {
        this->enable_access_log = enable;
        return *this;
    }
    
    ServerConfig& enableErrorLog(bool enable = true) {
        this->enable_error_log = enable;
        return *this;
    }
    
    ServerConfig& setMaxConnections(int max_conn) {
        this->max_connections = max_conn;
        return *this;
    }
    
    ServerConfig& setKeepAliveTimeout(int timeout) {
        this->keep_alive_timeout = timeout;
        return *this;
    }
    
    ServerConfig& setMaxRequestBodySize(size_t size) {
        this->max_request_body_size = size;
        return *this;
    }
};

inline std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::OFF:   return "OFF";
        default: return "UNKNOWN";
    }
}

} // namespace Gecko

#endif 