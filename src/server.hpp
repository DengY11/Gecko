#ifndef SERVER
#define SERVER
#include "http_request.hpp"
#include "http_response.hpp"
#include "thread_pool.hpp"
#include "io_thread_pool.hpp"
#include "server_config.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <sstream>
#include <functional>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <stack>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <string_view>

namespace Gecko {

class Context;

struct ConnectionInfo {
    int fd;
    std::string peer_addr;
    std::string local_addr;
    std::chrono::steady_clock::time_point last_active;
    std::chrono::steady_clock::time_point creation_time;
    std::atomic<bool> connected{true};
    std::atomic<size_t> request_count{0};
    std::string partial_request;  // 存储部分请求数据
    bool keep_alive{true};        // 是否保持连接
    
    ConnectionInfo(int fd, const std::string& peer, const std::string& local)
        : fd(fd), peer_addr(peer), local_addr(local),
          last_active(std::chrono::steady_clock::now()),
          creation_time(std::chrono::steady_clock::now()) {}
          
    void update_activity() {
        last_active = std::chrono::steady_clock::now();
    }
    
    bool is_expired(std::chrono::seconds timeout) const {
        auto now = std::chrono::steady_clock::now();
        return (now - last_active) > timeout;
    }
};

// 连接管理器 - 参考Drogon的HttpConnectionLimit
class ConnectionManager {
public:
    ConnectionManager(size_t max_connections = 10000, 
                     std::chrono::seconds keep_alive_timeout = std::chrono::seconds(60))
        : max_connections_(max_connections), keep_alive_timeout_(keep_alive_timeout) {
        // 预分配连接槽位，减少动态分配
        connections_.reserve(max_connections);
    }
    
    std::shared_ptr<ConnectionInfo> add_connection(int fd, const std::string& peer_addr, 
                                                  const std::string& local_addr);
    void remove_connection(int fd);
    void update_activity(int fd);
    std::shared_ptr<ConnectionInfo> get_connection(int fd);
    std::vector<int> get_expired_connections();
    size_t get_active_count() const { return active_connections_.load(); }
    bool can_accept_connection() const { return active_connections_.load() < max_connections_; }
    
    // 新增：批量操作接口
    void batch_remove_connections(const std::vector<int>& fds);
    void get_connection_stats(size_t& active, size_t& total_ever_created) const;
    
private:
    size_t max_connections_;
    std::chrono::seconds keep_alive_timeout_;
    
    // 优化：使用读写锁提高并发性能
    mutable std::shared_mutex connections_mutex_;
    std::unordered_map<int, std::shared_ptr<ConnectionInfo>> connections_;
    std::atomic<size_t> active_connections_{0};
    std::atomic<size_t> total_connections_created_{0};
};

class Server{
public:
    using RequestHandler = std::function<void(Context&)>;

    explicit Server(int port, size_t thread_pool_size = 0, size_t io_thread_count = 0)
        : port_(port), host_("0.0.0.0"), listen_fd_(-1), epoll_fd_(-1), 
          thread_pool_(std::make_unique<ThreadPool>(thread_pool_size)),
          io_thread_pool_(std::make_unique<IOThreadPool>(io_thread_count)),
          conn_manager_(std::make_unique<ConnectionManager>()) {
        print_server_info();
        epoll_fd_ = epoll_create1(0);
        if(epoll_fd_ == -1){
            throw std::runtime_error("epoll_create1 error");
        }
        setup_listen_socket();
    }
    
    explicit Server(const ServerConfig& config)
        : port_(config.port), host_(config.host), listen_fd_(-1), epoll_fd_(-1),
          thread_pool_(std::make_unique<ThreadPool>(config.thread_pool_size)),
          io_thread_pool_(std::make_unique<IOThreadPool>(config.io_thread_count)),
          conn_manager_(std::make_unique<ConnectionManager>(config.max_connections, 
                                                          std::chrono::seconds(config.keep_alive_timeout))),
          enable_performance_monitoring_(config.enable_performance_monitor),
          performance_monitor_interval_(config.performance_monitor_interval)
        {
        print_server_info_with_config(config);
        epoll_fd_ = epoll_create1(0);
        if(epoll_fd_ == -1){
            throw std::runtime_error("epoll_create1 error");
        }
        setup_listen_socket();
    }
    
    ~Server(){
        cleanup_all_connections();
        if(listen_fd_ != -1){
            close(listen_fd_);
        }
        if(epoll_fd_ != -1){
            close(epoll_fd_);
        }
    }

    void run(RequestHandler request_handler);
    
    size_t get_active_connections() const { return conn_manager_->get_active_count(); }
    size_t get_total_requests() const { return total_requests_.load(); }
    
    struct PerformanceStats {
        size_t requests_per_second = 0;
        size_t active_connections = 0;
        size_t total_requests = 0;
        size_t total_connections = 0;
        double avg_response_time_ms = 0.0;
        size_t io_thread_load = 0;
        size_t worker_thread_load = 0;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    PerformanceStats get_performance_stats() const;
    void print_performance_stats() const;
    void start_performance_monitoring(std::chrono::seconds interval = std::chrono::seconds(10));
    void stop_performance_monitoring();

protected:
    size_t find_content_length_in_headers(std::string_view headers_part) const;
    bool is_request_complete(const std::string& request_data) const;

private:
    void print_server_info();
    void print_server_info_with_config(const ServerConfig& config);
    void setup_listen_socket();
    
    void on_connection(int client_fd);
    void on_disconnect(int client_fd);
    void handler_new_connection();
    void handler_client_data(int client_fd);
    void cleanup_expired_connections();
    void cleanup_all_connections();
    
    // 新的三线程架构处理函数
    void process_request_with_io_thread(std::shared_ptr<ConnectionInfo> conn_info, const std::string& request_data);
    void handle_keep_alive_response(std::shared_ptr<ConnectionInfo> conn_info, const std::string& response_data);
    
    // 错误处理函数
    void send_error_response(int client_fd, int status_code, const std::string& message);
    
    // 网络工具函数
    void set_non_blockint(int fd);
    void add_to_epoll(int fd,uint32_t events);
    void remove_from_epoll(int fd);
    void send_response(int client_fd, const std::string& response);
    
    // 工具函数
    std::string get_peer_address(int fd) const;
    std::string get_local_address(int fd) const;

    static constexpr int MAX_EVENTS = 100000;
    int port_;
    std::string host_;
    int listen_fd_;
    int epoll_fd_;
    RequestHandler request_handler_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<IOThreadPool> io_thread_pool_;  // 新增：IO线程池
    std::unique_ptr<ConnectionManager> conn_manager_;
    
    // 统计信息
    std::atomic<size_t> total_requests_{0};
    std::atomic<size_t> total_connections_{0};
    
    // 新增：详细性能统计
    std::atomic<size_t> successful_requests_{0};
    std::atomic<size_t> failed_requests_{0};
    std::atomic<double> total_response_time_ms_{0.0};
    mutable std::atomic<size_t> last_requests_snapshot_{0};
    mutable std::chrono::steady_clock::time_point last_stats_snapshot_;
    
    // 性能监控线程
    bool enable_performance_monitoring_ = false;
    std::chrono::seconds performance_monitor_interval_;
    std::unique_ptr<std::thread> performance_monitor_thread_;
    std::atomic<bool> performance_monitoring_{false};
    
    // 性能监控
    mutable std::mutex stats_mutex_;
    std::chrono::steady_clock::time_point last_stats_time_;
    size_t last_request_count_{0};
    
    // 服务器状态
    std::atomic<bool> running_{false};
};

}

#endif
