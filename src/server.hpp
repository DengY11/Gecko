#ifndef SERVER
#define SERVER
#include "http_request.hpp"
#include "http_response.hpp"
#include "thread_pool.hpp"
#include "server_config.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <sstream>
#include <functional>
#include <memory>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <string_view>

namespace Gecko {

class Context;

class Server{
public:
    using RequestHandler = std::function<void(Context&)>;

    explicit Server(int port, size_t thread_pool_size = 0)
        : port_(port), host_("0.0.0.0"), listen_fd_(-1), epoll_fd_(-1), 
          thread_pool_(std::make_unique<ThreadPool>(thread_pool_size)) {
        print_server_info();
        epoll_fd_ = epoll_create1(0);
        if(epoll_fd_ == -1){
            throw std::runtime_error("epoll_create1 error");
        }
        setup_listen_socket();
    }
    
    explicit Server(const ServerConfig& config)
        : port_(config.port), host_(config.host), listen_fd_(-1), epoll_fd_(-1),
          thread_pool_(std::make_unique<ThreadPool>(config.thread_pool_size)) {
        print_server_info_with_config(config);
        epoll_fd_ = epoll_create1(0);
        if(epoll_fd_ == -1){
            throw std::runtime_error("epoll_create1 error");
        }
        setup_listen_socket();
    }
    
    ~Server(){
        if(listen_fd_ != -1){
            close(listen_fd_);
        }
        if(epoll_fd_ != -1){
            close(epoll_fd_);
        }
    }

    void run(RequestHandler request_handler);
    

protected:
    size_t find_content_length_in_headers(std::string_view headers_part) const;
    bool is_request_complete(const std::string& request_data) const;

private:
    void print_server_info();
    void print_server_info_with_config(const ServerConfig& config);
    void setup_listen_socket();
    void handler_new_connection();
    void handler_client_data(int client_fd);
    void process_request_async(int client_fd, std::string request_data);
    void set_non_blockint(int fd);
    void add_to_epoll(int fd,uint32_t events);
    void remove_from_epoll(int fd);
    void send_response(int client_fd, const std::string& response);

    static constexpr int MAX_EVENTS = 100000;
    int port_;
    std::string host_;
    int listen_fd_;
    int epoll_fd_;
    RequestHandler request_handler_;
    std::unique_ptr<ThreadPool> thread_pool_;
};

}

#endif
