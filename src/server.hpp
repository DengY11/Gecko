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
#include <chrono>

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

// 连接信息结构体 - 参考Drogon的TcpConnection概念
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
        : max_connections_(max_connections), keep_alive_timeout_(keep_alive_timeout) {}
    
    std::shared_ptr<ConnectionInfo> add_connection(int fd, const std::string& peer_addr, 
                                                  const std::string& local_addr);
    void remove_connection(int fd);
    void update_activity(int fd);
    std::shared_ptr<ConnectionInfo> get_connection(int fd);
    std::vector<int> get_expired_connections();
    size_t get_active_count() const { return active_connections_.load(); }
    bool can_accept_connection() const { return active_connections_.load() < max_connections_; }
    
private:
    size_t max_connections_;
    std::chrono::seconds keep_alive_timeout_;
    std::mutex connections_mutex_;
    std::unordered_map<int, std::shared_ptr<ConnectionInfo>> connections_;
    std::atomic<size_t> active_connections_{0};
};

class Server{
public:
    using RequestHandler = std::function<void(Context&)>;

    explicit Server(int port, size_t thread_pool_size = 0, size_t io_thread_count = 2)
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
                                                          std::chrono::seconds(config.keep_alive_timeout))) {
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
    
    // 连接统计信息
    size_t get_active_connections() const { return conn_manager_->get_active_count(); }
    size_t get_total_requests() const { return total_requests_.load(); }
    

protected:
    size_t find_content_length_in_headers(std::string_view headers_part) const;
    bool is_request_complete(const std::string& request_data) const;

private:
    void print_server_info();
    void print_server_info_with_config(const ServerConfig& config);
    void setup_listen_socket();
    
    // 连接处理 - 参考Drogon的onConnection
    void on_connection(int client_fd);
    void on_disconnect(int client_fd);
    void handler_new_connection();
    void handler_client_data(int client_fd);
    void cleanup_expired_connections();
    void cleanup_all_connections();
    
    // 新的三线程架构处理函数
    void process_request_with_io_thread(std::shared_ptr<ConnectionInfo> conn_info, const std::string& request_data);
    void handle_keep_alive_response(std::shared_ptr<ConnectionInfo> conn_info, const std::string& response_data);
    
    // 已弃用的函数（保留以防编译错误）
    void process_request_async(std::shared_ptr<ConnectionInfo> conn_info, std::string request_data);
    void process_data_in_worker(std::shared_ptr<ConnectionInfo> conn_info, const std::string& initial_data);
    bool read_more_data_in_worker(std::shared_ptr<ConnectionInfo> conn_info);
    void process_complete_request_in_worker(std::shared_ptr<ConnectionInfo> conn_info);
    void handle_connection_error(int client_fd, const std::string& error_msg);
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
    
    // 性能监控
    mutable std::mutex stats_mutex_;
    std::chrono::steady_clock::time_point last_stats_time_;
    size_t last_request_count_{0};
    
    // 服务器状态
    std::atomic<bool> running_{false};
};

}

#endif
