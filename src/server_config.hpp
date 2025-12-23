#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <string>
#include <cstddef>
#include <thread>
#include "http_request.hpp"

namespace Gecko {

struct ServerConfig {
    int port = 8080;                    /* Listen port */
    std::string host = "0.0.0.0";      /* Bind address */
    size_t thread_pool_size = 0;       /* Worker threads (0 = hw concurrency) */
    size_t io_thread_count = 16;       /* IO threads */
        
    int max_connections = 10000;        /* Max connections */
    int keep_alive_timeout = 30;        /* Keep-Alive timeout (seconds) */
    size_t max_request_body_size = 1024 * 1024; /* Max body size (1MB) */

    bool enable_performance_monitor = false;
    std::chrono::seconds performance_monitor_interval = std::chrono::seconds(10); /* Monitor interval */
    bool enable_cooperative_tasks = false;
    std::chrono::milliseconds cooperative_task_time_slice{2}; /* Default time slice for cooperative scheduling */
    int cooperative_task_priority = 0; /* -1 low, 0 normal, 1 high */

    enum class AcceptStrategy {
        SINGLE,          /* Single accept */
        BATCH_SIMPLE,    /* Simple batched accept */
    };
    AcceptStrategy accept_strategy = AcceptStrategy::BATCH_SIMPLE;
    int max_batch_accept = 128;  /* Max batched accept */
    
    ServerConfig() {
        if (thread_pool_size == 0) {
            thread_pool_size = std::thread::hardware_concurrency();
            if (thread_pool_size == 0) {
                thread_pool_size = 4; /* Fallback default */
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

    ServerConfig& enablePerformanceMonitoring(int interval = 10) {
        this->enable_performance_monitor = true;
        this->performance_monitor_interval = std::chrono::seconds(interval);
        return *this;
    }

    ServerConfig& enableCooperativeScheduling(int time_slice_ms = 2, int priority = 0) {
        this->enable_cooperative_tasks = true;
        this->cooperative_task_time_slice = std::chrono::milliseconds(time_slice_ms);
        this->cooperative_task_priority = priority;
        return *this;
    }


};

} /* namespace Gecko */

#endif 
