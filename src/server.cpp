#include "server.hpp"
#include <algorithm>
#include <cctype>

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
                handler_new_connection();
            } else if (events[i].events & EPOLLIN) {
                handler_client_data(events[i].data.fd);
            } else if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                std::cout << "Client disconnected: fd=" << events[i].data.fd << std::endl;
                remove_from_epoll(events[i].data.fd);
                close(events[i].data.fd);
            }
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
    set_non_blockint(listen_fd_);
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
    if (listen(listen_fd_, SOMAXCONN) < 0) {
        close(listen_fd_);
        throw std::runtime_error("Failed to listen: " + std::string(strerror(errno)));
    }
    add_to_epoll(listen_fd_, EPOLLIN);
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
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::cout << "New connection from " << client_ip << ":" << ntohs(client_addr.sin_port) 
              << " (fd=" << client_fd << ")" << std::endl;
    set_non_blockint(client_fd);
    add_to_epoll(client_fd, EPOLLIN);
}

void Server::handler_client_data(int client_fd) {
    const size_t BUFFER_SIZE = 8192;
    char buffer[BUFFER_SIZE];
    std::string request_data;
    
    while (true) {
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            request_data += buffer;
            
            // 检查是否找到了header结束标记
            size_t header_end_pos = request_data.find("\r\n\r\n");
            if (header_end_pos != std::string::npos) {
                // 找到了header结束标记，现在检查body是否完整
                size_t body_start = header_end_pos + 4; // 4是"\r\n\r\n"的长度
                
                // 简单解析Content-Length（避免在这里做完整的header解析）
                size_t content_length = 0;
                
                // 支持大小写不敏感的Content-Length查找
                std::string headers_part = request_data.substr(0, header_end_pos);
                std::string headers_lower = headers_part;
                std::transform(headers_lower.begin(), headers_lower.end(), headers_lower.begin(), ::tolower);
                
                size_t content_length_pos = headers_lower.find("content-length:");
                if (content_length_pos != std::string::npos) {
                    // 找到Content-Length header
                    size_t value_start = content_length_pos + 15; // "content-length:"的长度
                    size_t line_end = headers_part.find("\r\n", value_start);
                    if (line_end != std::string::npos) {
                        std::string length_str = headers_part.substr(value_start, line_end - value_start);
                        // 去除空格
                        length_str.erase(0, length_str.find_first_not_of(" \t"));
                        length_str.erase(length_str.find_last_not_of(" \t") + 1);
                        
                        try {
                            content_length = std::stoull(length_str);
                        } catch (const std::exception&) {
                            // Content-Length解析失败，假设为0
                            content_length = 0;
                        }
                    }
                }
                
                // 检查是否已经读取了足够的body数据
                size_t current_body_length = request_data.length() - body_start;
                if (current_body_length >= content_length) {
                    // 请求完整，退出读取循环
                    break;
                }
                // 否则继续读取body数据
            }
        } else if (bytes_read == 0) {
            std::cout << "Client closed connection: fd=" << client_fd << std::endl;
            remove_from_epoll(client_fd);
            close(client_fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                perror("read");
                remove_from_epoll(client_fd);
                close(client_fd);
                return;
            }
        }
    }

    if (request_data.empty()) {
        return;  
    }
    
    try {
        HttpRequest request;
        HttpRequestParser::parse(request_data, &request);
        std::cout << "[REQUEST] " << HttpMethodToString(request.getMethod()) 
                  << " " << request.getUrl() << std::endl;
        HttpResponse response = request_handler_(request);
        std::string response_str = HttpResponseSerializer::serialize(response);
        send_response(client_fd, response_str);
        std::cout << "[RESPONSE] " << response.getStatusCode() 
                  << " | " << response_str.length() << " bytes" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error processing request: " << e.what() << std::endl;
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

} // namespace Gecko