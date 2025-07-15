#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <string>
#include <cstddef>
#include <thread>

namespace Gecko {

// 服务器配置结构体 - 纯粹的服务器配置，不包含logger
struct ServerConfig {
    int port = 8080;                    // 监听端口
    std::string host = "0.0.0.0";      // 监听地址
    size_t thread_pool_size = 0;       // 工作线程数，0表示使用硬件并发数
    size_t io_thread_count = 8;        // IO线程数，默认8个（提高IO并发）
        
    int max_connections = 10000;        // 最大连接数
    int keep_alive_timeout = 30;        // Keep-Alive超时时间（秒）
    size_t max_request_body_size = 1024 * 1024; // 最大请求体大小（1MB）
    
    ServerConfig() {
        if (thread_pool_size == 0) {
            thread_pool_size = std::thread::hardware_concurrency();
            if (thread_pool_size == 0) {
                thread_pool_size = 4; // 后备默认值
            }
        }
    }
    
    explicit ServerConfig(int port) : ServerConfig() {
        this->port = port;
    }
    
    ServerConfig(int port, size_t threads) : ServerConfig(port) {
        this->thread_pool_size = threads;
    }
    
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
    
    ServerConfig& setIOThreadCount(size_t count) {
        this->io_thread_count = count;
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

} // namespace Gecko

#endif 
