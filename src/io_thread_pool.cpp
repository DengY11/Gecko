#include "io_thread_pool.hpp"
#include "server.hpp"
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace Gecko {

IOThreadPool::IOThreadPool(size_t io_thread_count) : stop_flag_(false) {
    if (io_thread_count == 0) {
        io_thread_count = 2; // 默认2个IO线程
    }
    
    std::cout << "🔄 创建异步IO线程池，IO线程数量: " << io_thread_count << std::endl;
    
    // 创建IO线程
    for (size_t i = 0; i < io_thread_count; ++i) {
        auto io_thread = std::make_unique<IOThread>();
        
        // 创建epoll
        io_thread->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (io_thread->epoll_fd == -1) {
            throw std::runtime_error("Failed to create epoll fd for IO thread: " + std::string(strerror(errno)));
        }
        
        // 创建唤醒管道
        if (pipe2(io_thread->wakeup_fd, O_CLOEXEC | O_NONBLOCK) == -1) {
            throw std::runtime_error("Failed to create wakeup pipe: " + std::string(strerror(errno)));
        }
        
        // 将唤醒管道的读端加入epoll
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = io_thread->wakeup_fd[0];
        if (epoll_ctl(io_thread->epoll_fd, EPOLL_CTL_ADD, io_thread->wakeup_fd[0], &ev) == -1) {
            throw std::runtime_error("Failed to add wakeup pipe to epoll: " + std::string(strerror(errno)));
        }
        
        // 启动IO线程 - 使用安全的指针捕获
        IOThread* thread_ptr = io_thread.get();
        io_thread->thread = std::thread([this, thread_ptr]() {
            io_reactor_loop(*thread_ptr);
        });
        
        io_threads_.push_back(std::move(io_thread));
    }
}

IOThreadPool::~IOThreadPool() {
    stop();
    std::cout << "🔄 异步IO线程池已关闭" << std::endl;
}

void IOThreadPool::stop() {
    stop_flag_ = true;
    
    // 停止所有IO线程
    for (auto& io_thread : io_threads_) {
        io_thread->running = false;
        wakeup_thread(*io_thread);
        if (io_thread->thread.joinable()) {
            io_thread->thread.join();
        }
    }
    
    io_threads_.clear();
}

void IOThreadPool::register_read(std::shared_ptr<ConnectionInfo> conn_info, 
                                std::function<void(std::shared_ptr<ConnectionInfo>, const std::string&)> callback) {
    if (stop_flag_ || !conn_info || !conn_info->connected) {
        return;
    }
    
    // 选择IO线程（轮询）
    int thread_idx = get_next_thread_index();
    auto& io_thread = *io_threads_[thread_idx];
    
    IOEvent event;
    event.fd = conn_info->fd;
    event.operation = IOOperation::READ;
    event.conn_info = conn_info;
    event.read_callback = callback;
    
    {
        std::lock_guard<std::mutex> lock(io_thread.events_mutex);
        io_thread.pending_events.push(event);
    }
    
    wakeup_thread(io_thread);
}

void IOThreadPool::async_write(std::shared_ptr<ConnectionInfo> conn_info, const std::string& data) {
    async_write(conn_info, data, nullptr);
}

void IOThreadPool::async_write(std::shared_ptr<ConnectionInfo> conn_info, const std::string& data, 
                              std::function<void(std::shared_ptr<ConnectionInfo>, bool)> callback) {
    if (stop_flag_ || !conn_info || !conn_info->connected) {
        if (callback) {
            callback(conn_info, false);
        }
        return;
    }
    
    // 选择IO线程（轮询）
    int thread_idx = get_next_thread_index();
    auto& io_thread = *io_threads_[thread_idx];
    
    IOEvent event;
    event.fd = conn_info->fd;
    event.operation = IOOperation::WRITE;
    event.conn_info = conn_info;
    event.write_data = data;
    event.write_callback = callback;
    
    {
        std::lock_guard<std::mutex> lock(io_thread.events_mutex);
        io_thread.pending_events.push(event);
    }
    
    wakeup_thread(io_thread);
}

void IOThreadPool::unregister_connection(std::shared_ptr<ConnectionInfo> conn_info) {
    if (!conn_info) return;
    
    // 从所有IO线程中移除该连接
    for (auto& io_thread : io_threads_) {
        std::lock_guard<std::mutex> lock(io_thread->events_mutex);
        io_thread->connections.erase(conn_info->fd);
        io_thread->read_callbacks.erase(conn_info->fd);
        io_thread->write_buffers.erase(conn_info->fd); // 清理写入缓冲区
        
        // 从epoll中移除
        epoll_ctl(io_thread->epoll_fd, EPOLL_CTL_DEL, conn_info->fd, nullptr);
    }
}

void IOThreadPool::io_reactor_loop(IOThread& io_thread) {
    const int max_events = 1000;
    struct epoll_event events[max_events];
    
    while (io_thread.running && !stop_flag_) {
        // 处理待处理的事件
        process_pending_events(io_thread);
        
        // 等待epoll事件（有事件处理或超时检查）
        int timeout = (io_thread.pending_events.empty()) ? 10 : 0; // 有待处理事件时非阻塞，否则10ms超时
        int num_events = epoll_wait(io_thread.epoll_fd, events, max_events, timeout);
        
        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "❌ epoll_wait error: " << strerror(errno) << std::endl;
            break;
        }
        
        for (int i = 0; i < num_events; ++i) {
            int fd = events[i].data.fd;
            
            if (fd == io_thread.wakeup_fd[0]) {
                // 唤醒信号，清空管道
                char buffer[256];
                while (read(fd, buffer, sizeof(buffer)) > 0) {
                    // 继续读取直到EAGAIN
                }
                continue;
            }
            
            // 处理连接事件
            if (events[i].events & EPOLLIN) {
                handle_read_event(io_thread, fd);
            }
            
            if (events[i].events & EPOLLOUT) {
                handle_write_ready(io_thread, fd);
            }
            
            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                // 连接出错或关闭
                auto conn_it = io_thread.connections.find(fd);
                if (conn_it != io_thread.connections.end()) {
                    conn_it->second->connected = false;
                    io_thread.connections.erase(conn_it);
                    io_thread.read_callbacks.erase(fd);
                    io_thread.write_buffers.erase(fd); // 清理写入缓冲区
                }
            }
        }
    }
}

void IOThreadPool::process_pending_events(IOThread& io_thread) {
    std::queue<IOEvent> events_to_process;
    
    {
        std::lock_guard<std::mutex> lock(io_thread.events_mutex);
        events_to_process.swap(io_thread.pending_events);
    }
    
    while (!events_to_process.empty()) {
        IOEvent event = events_to_process.front();
        events_to_process.pop();
        
        if (event.operation == IOOperation::READ) {
            // 检查是否已经注册过
            if (io_thread.connections.find(event.fd) != io_thread.connections.end()) {
                // 已经注册过，只更新回调
                io_thread.read_callbacks[event.fd] = event.read_callback;
                continue;
            }
            
            // 注册读取监听
            io_thread.connections[event.fd] = event.conn_info;
            io_thread.read_callbacks[event.fd] = event.read_callback;
            
            // 添加到epoll监听
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLET;  // 边缘触发
            ev.data.fd = event.fd;
            if (epoll_ctl(io_thread.epoll_fd, EPOLL_CTL_ADD, event.fd, &ev) == -1) {
                if (errno != EEXIST) {
                    std::cerr << "❌ epoll_ctl ADD failed for fd " << event.fd << ": " << strerror(errno) << std::endl;
                    // 清理注册信息
                    io_thread.connections.erase(event.fd);
                    io_thread.read_callbacks.erase(event.fd);
                }
            }
            
        } else if (event.operation == IOOperation::WRITE) {
            // 立即处理写入
            handle_write_event(io_thread, event);
        }
    }
}

void IOThreadPool::handle_read_event(IOThread& io_thread, int fd) {
    auto conn_it = io_thread.connections.find(fd);
    auto callback_it = io_thread.read_callbacks.find(fd);
    
    if (conn_it == io_thread.connections.end() || callback_it == io_thread.read_callbacks.end()) {
        return;
    }
    
    auto conn_info = conn_it->second;
    auto callback = callback_it->second;
    
    if (!conn_info || !conn_info->connected) {
        return;
    }
    
    // 非阻塞读取
    const size_t BUFFER_SIZE = 8192;
    char buffer[BUFFER_SIZE];
    std::string complete_data;
    
    while (true) {
        ssize_t bytes_read = read(fd, buffer, BUFFER_SIZE - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            complete_data += buffer;
            conn_info->update_activity();
            total_reads_++;
            
            // 检查是否已经读取完整请求
            if (complete_data.find("\r\n\r\n") != std::string::npos) {
                // 检查Content-Length
                size_t header_end = complete_data.find("\r\n\r\n");
                std::string headers = complete_data.substr(0, header_end);
                
                size_t content_length = 0;
                size_t pos = headers.find("Content-Length:");
                if (pos != std::string::npos) {
                    pos += 15;
                    while (pos < headers.length() && std::isspace(headers[pos])) pos++;
                    size_t end_pos = pos;
                    while (end_pos < headers.length() && std::isdigit(headers[end_pos])) end_pos++;
                    if (end_pos > pos) {
                        content_length = std::stoul(headers.substr(pos, end_pos - pos));
                    }
                }
                
                size_t current_body_size = complete_data.length() - header_end - 4;
                if (current_body_size >= content_length) {
                    // 请求完整，调用回调处理
                    callback(conn_info, complete_data);
                    // 不return，让连接继续在epoll中监听下一个请求
                    return;
                }
            }
        } else if (bytes_read == 0) {
            // 连接关闭
            conn_info->connected = false;
            io_thread.connections.erase(fd);
            io_thread.read_callbacks.erase(fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 数据读取完毕，等待下次EPOLLIN事件
                break;
            } else {
                // 读取错误
                std::cerr << "❌ Read error on fd " << fd << ": " << strerror(errno) << std::endl;
                conn_info->connected = false;
                io_thread.connections.erase(fd);
                io_thread.read_callbacks.erase(fd);
                return;
            }
        }
    }
}

void IOThreadPool::handle_write_event(IOThread& io_thread, const IOEvent& event) {
    auto conn_info = event.conn_info;
    if (!conn_info || !conn_info->connected) {
        if (event.write_callback) {
            event.write_callback(conn_info, false);
        }
        return;
    }
    
    // 创建写入缓冲区
    auto write_buffer = std::make_shared<WriteBuffer>();
    write_buffer->data = event.write_data;
    write_buffer->callback = event.write_callback;
    
    // 尝试立即写入
    if (try_write_immediate(io_thread, conn_info->fd, write_buffer)) {
        // 写入完成
        total_writes_++;
        if (write_buffer->callback) {
            write_buffer->callback(conn_info, true);
        }
    } else {
        // 部分写入或需要等待，注册EPOLLOUT事件
        io_thread.write_buffers[conn_info->fd] = write_buffer;
        
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;  // 同时监听读写
        ev.data.fd = conn_info->fd;
        
        if (epoll_ctl(io_thread.epoll_fd, EPOLL_CTL_MOD, conn_info->fd, &ev) == -1) {
            std::cerr << "❌ epoll_ctl MOD for EPOLLOUT failed on fd " << conn_info->fd 
                      << ": " << strerror(errno) << std::endl;
            // 写入失败
            io_thread.write_buffers.erase(conn_info->fd);
            if (write_buffer->callback) {
                write_buffer->callback(conn_info, false);
            }
        }
    }
}

bool IOThreadPool::try_write_immediate(IOThread& io_thread, int fd, std::shared_ptr<WriteBuffer> buffer) {
    while (!buffer->is_complete()) {
        auto remaining = buffer->remaining();
        ssize_t bytes_sent = write(fd, remaining.data(), remaining.size());
        
        if (bytes_sent > 0) {
            buffer->offset += bytes_sent;
        } else if (bytes_sent == 0) {
            // 连接关闭
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 需要等待EPOLLOUT事件
                return false;
            } else {
                // 写入错误
                std::cerr << "❌ Write error on fd " << fd << ": " << strerror(errno) << std::endl;
                return false;
            }
        }
    }
    return true; // 完全写入
}

void IOThreadPool::handle_write_ready(IOThread& io_thread, int fd) {
    auto buffer_it = io_thread.write_buffers.find(fd);
    if (buffer_it == io_thread.write_buffers.end()) {
        return; // 没有待写入的数据
    }
    
    auto buffer = buffer_it->second;
    auto conn_it = io_thread.connections.find(fd);
    if (conn_it == io_thread.connections.end()) {
        io_thread.write_buffers.erase(buffer_it);
        return;
    }
    
    auto conn_info = conn_it->second;
    
    if (try_write_immediate(io_thread, fd, buffer)) {
        // 写入完成
        total_writes_++;
        io_thread.write_buffers.erase(buffer_it);
        
        // 恢复只监听读事件
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(io_thread.epoll_fd, EPOLL_CTL_MOD, fd, &ev);
        
        if (buffer->callback) {
            buffer->callback(conn_info, true);
        }
    } else {
        // 写入失败或连接关闭
        io_thread.write_buffers.erase(buffer_it);
        if (buffer->callback) {
            buffer->callback(conn_info, false);
        }
    }
}

void IOThreadPool::wakeup_thread(IOThread& io_thread) {
    char wake = 1;
    write(io_thread.wakeup_fd[1], &wake, 1);
}

int IOThreadPool::get_next_thread_index() {
    return round_robin_index_.fetch_add(1) % io_threads_.size();
}

} // namespace Gecko 