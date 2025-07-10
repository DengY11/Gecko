#include "server.hpp"
#include "context.hpp"
#include <algorithm>
#include <cctype>
#include <string_view>
#include <thread>

namespace Gecko {

void Server::print_server_info() {
    std::cout << "🦎 Gecko Web Framework" << std::endl;
    std::cout << "📝 Configuration:" << std::endl;
    std::cout << "   └─ Port: " << port_ << std::endl;
    std::cout << "   └─ Host: " << host_ << std::endl;
    std::cout << "   └─ Thread Pool Size: " << thread_pool_->thread_count() << std::endl;
    std::cout << "🚀 Server initializing..." << std::endl;
}

void Server::print_server_info_with_config(const ServerConfig& config) {
    std::cout << "🦎 Gecko Web Framework" << std::endl;
    std::cout << "📝 Configuration:" << std::endl;
    std::cout << "   ├─ Port: " << config.port << std::endl;
    std::cout << "   ├─ Host: " << config.host << std::endl;
    std::cout << "   ├─ Thread Pool Size: " << config.thread_pool_size << std::endl;
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
    std::cout << "✅ Server successfully started!" << std::endl;
    std::cout << "🌐 Listening on http://" << host_ << ":" << port_ << std::endl;
    std::cout << "💡 Press Ctrl+C to stop the server" << std::endl;
    std::cout << "📊 Ready to accept connections..." << std::endl;
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
            if (is_request_complete(request_data)) {
                break;
            }
        } else if (bytes_read == 0) {
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
    process_request_async(client_fd, std::move(request_data));
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

void Server::process_request_async(int client_fd, std::string request_data) {
    thread_pool_->enqueue([this, client_fd, request_data = std::move(request_data)]() {
        try {
            HttpRequest request;
            HttpRequestParser::parse(request_data, &request);
            
            Context ctx(request);
            request_handler_(ctx);
            HttpResponse response = ctx.response();
            
            std::string response_str = HttpResponseSerializer::serialize(response);
            send_response(client_fd, response_str);
        } catch (const std::exception& e) {
            std::cerr << "Error processing request: " << e.what() << std::endl;
            HttpResponse error_response = HttpResponse::stockResponse(500);
            error_response.setBody("Internal Server Error: " + std::string(e.what()));
            std::string response_str = HttpResponseSerializer::serialize(error_response);
            send_response(client_fd, response_str);
        }

        // HTTP/1.0 默认关闭连接
        // TODO: 支持HTTP/1.1的keep-alive
        // 注意：这里不能调用remove_from_epoll，因为我们在工作线程中
        // epoll操作应该在主线程中进行
        close(client_fd);
    });
}

} // namespace Gecko