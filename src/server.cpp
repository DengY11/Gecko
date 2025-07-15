#include "server.hpp"
#include "context.hpp"
#include <algorithm>
#include <cctype>
#include <string_view>
#include <thread>
#include <sstream>

namespace Gecko {

// ConnectionManager 实现
std::shared_ptr<ConnectionInfo> ConnectionManager::add_connection(int fd, 
                                                                const std::string& peer_addr, 
                                                                const std::string& local_addr) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    if (active_connections_.load() >= max_connections_) {
        return nullptr;
    }
    
    auto conn_info = std::make_shared<ConnectionInfo>(fd, peer_addr, local_addr);
    connections_[fd] = conn_info;
    active_connections_++;
    
    return conn_info;
}

void ConnectionManager::remove_connection(int fd) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        it->second->connected = false;
        connections_.erase(it);
        active_connections_--;
    }
}

void ConnectionManager::update_activity(int fd) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        it->second->update_activity();
    }
}

std::shared_ptr<ConnectionInfo> ConnectionManager::get_connection(int fd) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(fd);
    return (it != connections_.end()) ? it->second : nullptr;
}

std::vector<int> ConnectionManager::get_expired_connections() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    std::vector<int> expired;
    
    for (const auto& [fd, conn_info] : connections_) {
        if (conn_info->is_expired(keep_alive_timeout_)) {
            expired.push_back(fd);
        }
    }
    
    return expired;
}

// Server 实现
void Server::print_server_info() {
    std::cout << "🦎 Gecko Web Framework" << std::endl;
    std::cout << "📝 Configuration:" << std::endl;
    std::cout << "   └─ Port: " << port_ << std::endl;
    std::cout << "   └─ Host: " << host_ << std::endl;
    std::cout << "   └─ Worker Thread Pool Size: " << thread_pool_->thread_count() << std::endl;
    std::cout << "   └─ IO Thread Pool Size: " << io_thread_pool_->thread_count() << std::endl;
    std::cout << "   └─ Max Connections: " << 10000 << std::endl;
    std::cout << "🚀 Server initializing..." << std::endl;
}

void Server::print_server_info_with_config(const ServerConfig& config) {
    std::cout << "🦎 Gecko Web Framework" << std::endl;
    std::cout << "📝 Configuration:" << std::endl;
    std::cout << "   ├─ Port: " << config.port << std::endl;
    std::cout << "   ├─ Host: " << config.host << std::endl;
    std::cout << "   ├─ Worker Thread Pool Size: " << config.thread_pool_size << std::endl;
    std::cout << "   ├─ IO Thread Pool Size: " << config.io_thread_count << std::endl;
    std::cout << "   ├─ Max Connections: " << config.max_connections << std::endl;
    std::cout << "   ├─ Keep-Alive Timeout: " << config.keep_alive_timeout << "s" << std::endl;
    std::cout << "   └─ Max Request Body Size: " << (config.max_request_body_size / 1024) << "KB" << std::endl;
    std::cout << "🚀 Server initializing..." << std::endl;
}

void Server::run(RequestHandler request_handler) {
    this->request_handler_ = request_handler;
    if (!this->request_handler_) {
        throw std::runtime_error("Cannot run server with a null handler");
    }
    
    running_ = true;
    std::vector<struct epoll_event> events(MAX_EVENTS);
    
    std::cout << "🚀 Server started on " << host_ << ":" << port_ << std::endl;
    
    while (running_) {
        // 定期清理过期连接
        cleanup_expired_connections();
        
        int num_events = epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, 10); // 10ms超时，提高响应性
        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            continue;
        }
        
        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == listen_fd_) {
                handler_new_connection();
            }
            // 移除数据事件处理，因为现在由IO线程池完全负责
            // 错误事件也由IO线程池在各自的epoll中处理
        }
    }
}

void Server::setup_listen_socket() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
    }
    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(listen_fd_);
        throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
    }
    
    // 启用端口复用，提高并发性能
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        std::cerr << "⚠️ Failed to set SO_REUSEPORT: " << strerror(errno) << " (继续运行)" << std::endl;
    }
    
    // 设置TCP_NODELAY，减少延迟
    if (setsockopt(listen_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        std::cerr << "⚠️ Failed to set TCP_NODELAY: " << strerror(errno) << " (继续运行)" << std::endl;
    }
    
    // 调整发送和接收缓冲区大小
    int buffer_size = 64 * 1024; // 64KB
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size)) < 0) {
        std::cerr << "⚠️ Failed to set SO_SNDBUF: " << strerror(errno) << " (继续运行)" << std::endl;
    }
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size)) < 0) {
        std::cerr << "⚠️ Failed to set SO_RCVBUF: " << strerror(errno) << " (继续运行)" << std::endl;
    }
    set_non_blockint(listen_fd_);
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if (host_ == "0.0.0.0" || host_ == "*") {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
            close(listen_fd_);
            throw std::runtime_error("Invalid host address: " + host_);
        }
    }
    
    server_addr.sin_port = htons(port_);
    if (bind(listen_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_fd_);
        throw std::runtime_error("Failed to bind to " + host_ + ":" + std::to_string(port_) + 
                                ": " + std::string(strerror(errno)));
    }
    if (listen(listen_fd_, SOMAXCONN) < 0) {
        close(listen_fd_);
        throw std::runtime_error("Failed to listen: " + std::string(strerror(errno)));
    }
    add_to_epoll(listen_fd_, EPOLLIN);
}

// 连接处理 - 参考Drogon的onConnection
void Server::on_connection(int client_fd) {
    std::string peer_addr = get_peer_address(client_fd);
    std::string local_addr = get_local_address(client_fd);
    
    auto conn_info = conn_manager_->add_connection(client_fd, peer_addr, local_addr);
    if (!conn_info) {
        std::cerr << "⚠️  Connection limit reached, rejecting connection from " << peer_addr << std::endl;
        send_error_response(client_fd, 503, "Service Unavailable");
        close(client_fd);
        return;
    }
    
    total_connections_++;
    #ifdef DEBUG
    std::cout << "✅ New connection from " << peer_addr << " (fd: " << client_fd 
              << ", total: " << conn_manager_->get_active_count() << ")" << std::endl;
    #endif
    
    // 直接将新连接注册到IO线程池进行异步读取
    io_thread_pool_->register_read(conn_info, [this](std::shared_ptr<ConnectionInfo> conn_info, const std::string& request_data) {
        process_request_with_io_thread(conn_info, request_data);
    });
}

void Server::on_disconnect(int client_fd) {
    auto conn_info = conn_manager_->get_connection(client_fd);
    if (conn_info) {
        #ifdef DEBUG
        std::cout << "❌ Connection closed " << conn_info->peer_addr << " (fd: " << client_fd 
                  << ", requests: " << conn_info->request_count.load() << ")" << std::endl;
        #endif
    }
    
    conn_manager_->remove_connection(client_fd);
    remove_from_epoll(client_fd);
    close(client_fd);
}

void Server::handler_new_connection() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept");
        }
        return;
    }
    
    // 检查连接限制
    if (!conn_manager_->can_accept_connection()) {
        send_error_response(client_fd, 503, "Service Unavailable");
        close(client_fd);
        return;
    }
    
    set_non_blockint(client_fd);
    // 不再将新连接添加到主线程epoll，直接交给IO线程池管理
    on_connection(client_fd);
}

void Server::handler_client_data(int client_fd) {
    auto conn_info = conn_manager_->get_connection(client_fd);
    if (!conn_info || !conn_info->connected) {
        on_disconnect(client_fd);
        return;
    }
    
    conn_info->update_activity();
    
    // 新的三线程架构：直接提交IO任务到IO线程池
    io_thread_pool_->register_read(conn_info, [this](std::shared_ptr<ConnectionInfo> conn_info, const std::string& request_data) {
        // 在IO线程中读取完整数据后，提交到工作线程处理业务逻辑
        process_request_with_io_thread(conn_info, request_data);
    });
}

void Server::process_request_with_io_thread(std::shared_ptr<ConnectionInfo> conn_info, const std::string& request_data) {
    if (!conn_info || !conn_info->connected) {
        return;
    }
    
    conn_info->request_count++;
    total_requests_++;
    
    // 提交到工作线程处理业务逻辑
    thread_pool_->enqueue([this, conn_info, request_data]() {
        try {
            HttpRequest request;
            HttpRequestParser::parse(request_data, &request);
            
            // 检查是否支持keep-alive
            bool keep_alive = false;
            auto headers = request.getHeaders();
            auto connection_it = headers.find("Connection");
            std::string connection_header = (connection_it != headers.end()) ? connection_it->second : "";
            if (connection_header == "keep-alive" || 
                (HttpVersionToString(request.getVersion()) == "HTTP/1.1" && connection_header != "close")) {
                keep_alive = true;
            }
            conn_info->keep_alive = keep_alive;
            
            Context ctx(request);
            request_handler_(ctx);
            HttpResponse response = ctx.response();
            
            // 设置连接类型
            if (keep_alive) {
                response.addHeader("Connection", "keep-alive");
                response.addHeader("Keep-Alive", "timeout=30, max=100");
            } else {
                response.addHeader("Connection", "close");
            }
            
            std::string response_str = HttpResponseSerializer::serialize(response);
            
            // 使用IO线程池异步发送响应
            if (conn_info->connected) {
                handle_keep_alive_response(conn_info, response_str);
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Error processing request from " << conn_info->peer_addr 
                      << ": " << e.what() << std::endl;
            
            if (conn_info->connected) {
                std::string error_response = "HTTP/1.1 500 Internal Server Error\r\n"
                                           "Content-Type: text/plain\r\n"
                                           "Connection: close\r\n"
                                           "Content-Length: 21\r\n\r\n"
                                           "Internal Server Error";
                io_thread_pool_->async_write(conn_info, error_response, 
                    [this](std::shared_ptr<ConnectionInfo> conn, bool /*success*/) {
                        if (conn) {
                            on_disconnect(conn->fd);
                        }
                    });
                conn_info->keep_alive = false;
            }
        }
    });
}

void Server::handle_keep_alive_response(std::shared_ptr<ConnectionInfo> conn_info, const std::string& response_data) {
    if (!conn_info || !conn_info->connected) {
        return;
    }
    
    // 使用带回调的IO线程池异步写入响应
    io_thread_pool_->async_write(conn_info, response_data, 
        [this, conn_info](std::shared_ptr<ConnectionInfo> conn, bool success) {
            if (!conn || !conn->connected) {
                return;
            }
            
            if (success) {
                // 根据keep-alive状态决定是否关闭连接
                if (!conn->keep_alive) {
                    // 关闭连接
                    on_disconnect(conn->fd);
                }
            } else {
                // 写入失败，强制关闭连接
                on_disconnect(conn->fd);
            }
        });
}

void Server::process_data_in_worker(std::shared_ptr<ConnectionInfo> conn_info, const std::string& initial_data) {
    if (!conn_info || !conn_info->connected) {
        return;
    }
    
    // 将初始数据添加到部分请求缓存
    conn_info->partial_request += initial_data;
    
    // 检查是否需要读取更多数据
    if (!is_request_complete(conn_info->partial_request)) {
        // 在工作线程中继续读取数据
        if (!read_more_data_in_worker(conn_info)) {
            return; // 读取失败或连接关闭
        }
    }
    
    // 检查请求是否完整
    if (is_request_complete(conn_info->partial_request)) {
        // 处理完整请求
        process_complete_request_in_worker(conn_info);
    }
}

bool Server::read_more_data_in_worker(std::shared_ptr<ConnectionInfo> conn_info) {
    if (!conn_info || !conn_info->connected) {
        return false;
    }
    
    const size_t BUFFER_SIZE = 8192;
    char buffer[BUFFER_SIZE];
    
    while (!is_request_complete(conn_info->partial_request)) {
        ssize_t bytes_read = read(conn_info->fd, buffer, BUFFER_SIZE - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            conn_info->partial_request += buffer;
            conn_info->update_activity();
        } else if (bytes_read == 0) {
            // 连接关闭
            std::cerr << "❌ Connection closed while reading more data from " 
                      << conn_info->peer_addr << std::endl;
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 数据暂时不可用，等待下次epoll事件
                // 这种情况下请求还未完整，需要等待更多数据
                return true;
            } else {
                std::cerr << "❌ Read error from " << conn_info->peer_addr 
                          << ": " << strerror(errno) << std::endl;
                return false;
            }
        }
    }
    
    return true;
}

void Server::process_complete_request_in_worker(std::shared_ptr<ConnectionInfo> conn_info) {
    if (!conn_info || !conn_info->connected) {
        return;
    }
    
    conn_info->request_count++;
    total_requests_++;
    
    try {
        HttpRequest request;
        HttpRequestParser::parse(conn_info->partial_request, &request);
        
        Context ctx(request);
        request_handler_(ctx);
        HttpResponse response = ctx.response();
        
        std::string response_str = HttpResponseSerializer::serialize(response);
        
        // 确保连接仍然有效
        if (conn_info->connected) {
            send_response(conn_info->fd, response_str);
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Error processing request from " << conn_info->peer_addr 
                  << ": " << e.what() << std::endl;
        
        if (conn_info->connected) {
            send_error_response(conn_info->fd, 500, "Internal Server Error");
        }
    }
    
    // 清理请求数据
    conn_info->partial_request.clear();
    
    // HTTP/1.0 默认关闭连接
    // TODO: 支持HTTP/1.1的keep-alive
    on_disconnect(conn_info->fd);
}

void Server::process_request_async(std::shared_ptr<ConnectionInfo> conn_info, std::string request_data) {
    if (!conn_info || !conn_info->connected) {
        return;
    }
    
    conn_info->request_count++;
    total_requests_++;
    
    thread_pool_->enqueue([this, conn_info, request_data = std::move(request_data)]() {
        try {
            HttpRequest request;
            HttpRequestParser::parse(request_data, &request);
            
            Context ctx(request);
            request_handler_(ctx);
            HttpResponse response = ctx.response();
            
            std::string response_str = HttpResponseSerializer::serialize(response);
            
            // 确保连接仍然有效
            if (conn_info->connected) {
                send_response(conn_info->fd, response_str);
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Error processing request from " << conn_info->peer_addr 
                      << ": " << e.what() << std::endl;
            
            if (conn_info->connected) {
                send_error_response(conn_info->fd, 500, "Internal Server Error");
            }
        }

        // HTTP/1.0 默认关闭连接
        // TODO: 支持HTTP/1.1的keep-alive
        on_disconnect(conn_info->fd);
    });
}

void Server::handle_connection_error(int client_fd, const std::string& error_msg) {
    auto conn_info = conn_manager_->get_connection(client_fd);
    if (conn_info) {
        std::cerr << "❌ Connection error from " << conn_info->peer_addr << ": " << error_msg << std::endl;
    }
    on_disconnect(client_fd);
}

void Server::send_error_response(int client_fd, int status_code, const std::string& message) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << message << "\r\n";
    response << "Content-Type: text/plain\r\n";
    response << "Connection: close\r\n";
    response << "Content-Length: " << message.length() << "\r\n\r\n";
    response << message;
    
    send_response(client_fd, response.str());
}

void Server::cleanup_expired_connections() {
    static auto last_cleanup = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    // 每10秒清理一次
    if (now - last_cleanup < std::chrono::seconds(10)) {
        return;
    }
    
    auto expired = conn_manager_->get_expired_connections();
    for (int fd : expired) {
        #ifdef DEBUG    
        std::cout << "🧹 Cleaning up expired connection (fd: " << fd << ")" << std::endl;
        #endif
        on_disconnect(fd);
    }
    
    last_cleanup = now;
}


void Server::cleanup_all_connections() {
    running_ = false;
    
    // 获取所有活跃连接并关闭
    std::vector<int> all_fds;
    {
        // 这里需要访问ConnectionManager的私有成员，我们通过另一种方式实现
        while (conn_manager_->get_active_count() > 0) {
            auto expired = conn_manager_->get_expired_connections();
            for (int fd : expired) {
                on_disconnect(fd);
            }
            // 如果还有连接，强制清理
            if (conn_manager_->get_active_count() > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    #ifdef DEBUG
    std::cout << "🧹 All connections cleaned up" << std::endl;
    #endif
}

// 工具函数
std::string Server::get_peer_address(int fd) const {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(fd, (struct sockaddr*)&addr, &addr_len) == 0) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
        return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
    }
    return "unknown";
}

std::string Server::get_local_address(int fd) const {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr*)&addr, &addr_len) == 0) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
        return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
    }
    return "unknown";
}

void Server::set_non_blockint(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        throw std::runtime_error("fcntl F_GETFL failed: " + std::string(strerror(errno)));
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw std::runtime_error("fcntl F_SETFL failed: " + std::string(strerror(errno)));
    }
}

void Server::add_to_epoll(int fd, uint32_t events) {
    struct epoll_event event;
    event.events = events | EPOLLET;  
    event.data.fd = fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
        throw std::runtime_error("epoll_ctl ADD failed: " + std::string(strerror(errno)));
    }
}

void Server::remove_from_epoll(int fd) {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        if (errno != EBADF && errno != ENOENT) {
            perror("epoll_ctl DEL");
        }
    }
}

void Server::send_response(int client_fd, const std::string& response) {
    const char* data = response.c_str();
    size_t total_sent = 0;
    size_t total_size = response.length();
    while (total_sent < total_size) {
        ssize_t sent = write(client_fd, data + total_sent, total_size - total_sent);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 在生产环境中，应该使用EPOLLOUT事件来处理这种情况
                continue;
            } else {
                perror("write");
                break;
            }
        } else if (sent == 0) {
            break;
        } else {
            total_sent += sent;
        }
    }
}

size_t Server::find_content_length_in_headers(std::string_view headers_part) const {
    std::string headers_lower{headers_part};
    std::transform(headers_lower.begin(), headers_lower.end(), headers_lower.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    size_t content_length_pos = headers_lower.find("content-length:");
    if (content_length_pos == std::string::npos) {
        return 0; // don't find content-length
    }
    size_t value_start = content_length_pos + 15; // "content-length:"
    size_t line_end = headers_part.find("\r\n", value_start);
    if (line_end == std::string::npos) {
        line_end = headers_part.length();
    }
    std::string length_str{headers_part.substr(value_start, line_end - value_start)};
    length_str.erase(0, length_str.find_first_not_of(" \t"));
    length_str.erase(length_str.find_last_not_of(" \t") + 1);
    
    try {
        return std::stoull(length_str);
    } catch (const std::exception&) {
        //failed to parse content-length
        return 0;
    }
}

bool Server::is_request_complete(const std::string& request_data) const {
    size_t header_end_pos = request_data.find("\r\n\r\n");
    if (header_end_pos == std::string::npos) {
        return false;  
    }
    size_t body_start = header_end_pos + 4; // "\r\n\r\n"
    //std::string headers_part = request_data.substr(0, header_end_pos);
    std::string_view headers_part(request_data.data(), header_end_pos);
    size_t content_length = find_content_length_in_headers(headers_part);
    size_t current_body_length = request_data.length() - body_start;
    return current_body_length >= content_length;
}

} // namespace Gecko