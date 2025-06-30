#include "server.hpp"

namespace Gecko {

void Server::run(RequestHandler request_handler) {
    this->request_handler_ = request_handler;
    if (!this->request_handler_) {
        throw std::runtime_error("Cannot run server with a null handler");
    }
    
    std::cout << "🦎 Gecko Server listening on port " << port_ << "..." << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    
    std::vector<struct epoll_event> events(MAX_EVENTS);
    
    while (true) {
        int num_events = epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, -1);
        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            continue;
        }
        
        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == listen_fd_) {
                // 新连接
                handler_new_connection();
            } else if (events[i].events & EPOLLIN) {
                // 客户端数据可读
                handler_client_data(events[i].data.fd);
            } else if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                // 连接错误或关闭
                std::cout << "Client disconnected: fd=" << events[i].data.fd << std::endl;
                remove_from_epoll(events[i].data.fd);
                close(events[i].data.fd);
            }
        }
    }
}

void Server::setup_listen_socket() {
    // 1. 创建socket
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
    }

    // 2. 设置socket选项：地址重用
    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(listen_fd_);
        throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
    }

    // 3. 设置为非阻塞
    set_non_blockint(listen_fd_);

    // 4. 绑定地址和端口
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有接口
    server_addr.sin_port = htons(port_);

    if (bind(listen_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_fd_);
        throw std::runtime_error("Failed to bind to port " + std::to_string(port_) + 
                                ": " + std::string(strerror(errno)));
    }

    // 5. 开始监听
    if (listen(listen_fd_, SOMAXCONN) < 0) {
        close(listen_fd_);
        throw std::runtime_error("Failed to listen: " + std::string(strerror(errno)));
    }

    // 6. 添加到epoll
    add_to_epoll(listen_fd_, EPOLLIN);
}

void Server::handler_new_connection() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // 接受新连接（非阻塞）
    int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept");
        }
        return;
    }

    // 获取客户端IP
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::cout << "New connection from " << client_ip << ":" << ntohs(client_addr.sin_port) 
              << " (fd=" << client_fd << ")" << std::endl;

    // 设置客户端socket为非阻塞
    set_non_blockint(client_fd);

    // 添加到epoll监听
    add_to_epoll(client_fd, EPOLLIN);
}

void Server::handler_client_data(int client_fd) {
    const size_t BUFFER_SIZE = 8192;
    char buffer[BUFFER_SIZE];
    std::string request_data;

    // 读取所有数据
    while (true) {
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            request_data += buffer;
            
            // 检查是否读取完整的HTTP请求
            // 简单检查：是否包含完整的HTTP头部（以\r\n\r\n结尾）
            if (request_data.find("\r\n\r\n") != std::string::npos) {
                break;
            }
        } else if (bytes_read == 0) {
            // 客户端关闭连接
            std::cout << "Client closed connection: fd=" << client_fd << std::endl;
            remove_from_epoll(client_fd);
            close(client_fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有更多数据可读
                break;
            } else {
                // 读取错误
                perror("read");
                remove_from_epoll(client_fd);
                close(client_fd);
                return;
            }
        }
    }

    if (request_data.empty()) {
        return;  // 没有数据
    }

    try {
        // 解析HTTP请求
        HttpRequest request;
        HttpRequestParser::parse(request_data, &request);
        
        std::cout << "[REQUEST] " << HttpMethodToString(request.getMethod()) 
                  << " " << request.getUrl() << std::endl;

        // 调用业务处理函数
        HttpResponse response = request_handler_(request);

        // 发送响应
        std::string response_str = HttpResponseSerializer::serialize(response);
        send_response(client_fd, response_str);

        std::cout << "[RESPONSE] " << response.getStatusCode() 
                  << " | " << response_str.length() << " bytes" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error processing request: " << e.what() << std::endl;
        
        // 发送500错误响应
        HttpResponse error_response = HttpResponse::stockResponse(500);
        error_response.setBody("Internal Server Error: " + std::string(e.what()));
        std::string response_str = HttpResponseSerializer::serialize(error_response);
        send_response(client_fd, response_str);
    }

    // HTTP/1.0 默认关闭连接
    // TODO: 支持HTTP/1.1的keep-alive
    remove_from_epoll(client_fd);
    close(client_fd);
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
    event.events = events | EPOLLET;  // 使用边缘触发模式
    event.data.fd = fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
        throw std::runtime_error("epoll_ctl ADD failed: " + std::string(strerror(errno)));
    }
}

void Server::remove_from_epoll(int fd) {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        // 如果fd已经关闭，epoll_ctl可能会失败，这是正常的
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
                // 发送缓冲区满，稍后重试
                // 在生产环境中，应该使用EPOLLOUT事件来处理这种情况
                continue;
            } else {
                perror("write");
                break;
            }
        } else if (sent == 0) {
            // 连接被对方关闭
            break;
        } else {
            total_sent += sent;
        }
    }
}

} // namespace Gecko