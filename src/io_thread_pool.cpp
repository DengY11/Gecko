#include "io_thread_pool.hpp"
#include "server.hpp"
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <charconv>
#include <string_view>

namespace Gecko {

IOThreadPool::IOThreadPool(size_t io_thread_count) : stop_flag_(false) {
    if (io_thread_count == 0) {
        io_thread_count = std::max(4u, std::thread::hardware_concurrency() / 2); // Default to half the CPU cores, at least 4
    }
    
    std::cout << "[LOOP] Creating async IO thread pool, thread count: " << io_thread_count << std::endl;
    
    for (size_t i = 0; i < io_thread_count; ++i) {
        auto io_thread = std::make_unique<IOThread>();
        
        io_thread->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (io_thread->epoll_fd == -1) {
            throw std::runtime_error("Failed to create epoll fd for IO thread: " + std::string(strerror(errno)));
        }
        if (pipe2(io_thread->wakeup_fd, O_CLOEXEC | O_NONBLOCK) == -1) {
            throw std::runtime_error("Failed to create wakeup pipe: " + std::string(strerror(errno)));
        }
        
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = io_thread->wakeup_fd[0];
        if (epoll_ctl(io_thread->epoll_fd, EPOLL_CTL_ADD, io_thread->wakeup_fd[0], &ev) == -1) {
            throw std::runtime_error("Failed to add wakeup pipe to epoll: " + std::string(strerror(errno)));
        }
        
        IOThread* thread_ptr = io_thread.get();
        io_thread->thread = std::thread([this, thread_ptr]() {
            io_reactor_loop(*thread_ptr);
        });
        
        io_threads_.push_back(std::move(io_thread));
    }
}

IOThreadPool::~IOThreadPool() {
    stop();
    std::cout << "[LOOP] Async IO thread pool stopped" << std::endl;
}

void IOThreadPool::stop() {
    stop_flag_ = true;
    
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
    
    for (auto& io_thread : io_threads_) {
        std::lock_guard<std::mutex> lock(io_thread->events_mutex);
        io_thread->connections.erase(conn_info->fd);
        io_thread->read_callbacks.erase(conn_info->fd);
        io_thread->write_buffers.erase(conn_info->fd); 
        
        epoll_ctl(io_thread->epoll_fd, EPOLL_CTL_DEL, conn_info->fd, nullptr);
    }
}

void IOThreadPool::io_reactor_loop(IOThread& io_thread) {
    const int max_events = 1000;
    struct epoll_event events[max_events];
    
    while (io_thread.running && !stop_flag_) {
        process_pending_events(io_thread);
        
        int timeout = (io_thread.pending_events.empty()) ? 1 : 0;
        int num_events = epoll_wait(io_thread.epoll_fd, events, max_events, timeout);
        
        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "[ERROR] epoll_wait error: " << strerror(errno) << std::endl;
            break;
        }
        
        for (int i = 0; i < num_events; ++i) {
            int fd = events[i].data.fd;
            
            if (fd == io_thread.wakeup_fd[0]) {
                char buffer[256];
                while (read(fd, buffer, sizeof(buffer)) > 0) {
                }
                continue;
            }
            
            if (events[i].events & EPOLLIN) {
                handle_read_event(io_thread, fd);
            }
            
            if (events[i].events & EPOLLOUT) {
                handle_write_ready(io_thread, fd);
            }
            
            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                auto conn_it = io_thread.connections.find(fd);
                if (conn_it != io_thread.connections.end()) {
                    conn_it->second->connected = false;
                    io_thread.connections.erase(conn_it);
                    io_thread.read_callbacks.erase(fd);
                    io_thread.write_buffers.erase(fd); 
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
            if (io_thread.connections.find(event.fd) != io_thread.connections.end()) {
                io_thread.read_callbacks[event.fd] = event.read_callback;
                continue;
            }
            
            io_thread.connections[event.fd] = event.conn_info;
            io_thread.read_callbacks[event.fd] = event.read_callback;
            
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLET;  
            ev.data.fd = event.fd;
            if (epoll_ctl(io_thread.epoll_fd, EPOLL_CTL_ADD, event.fd, &ev) == -1) {
                if (errno != EEXIST) {
                    std::cerr << "epoll_ctl ADD failed for fd " << event.fd << ": " << strerror(errno) << std::endl;
                    io_thread.connections.erase(event.fd);
                    io_thread.read_callbacks.erase(event.fd);
                }
            }
            
        } else if (event.operation == IOOperation::WRITE) {
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
    
    const size_t BUFFER_SIZE = 16384; 
    char buffer[BUFFER_SIZE];
    std::string complete_data;
    complete_data.reserve(8192); 
    
    while (true) {
        ssize_t bytes_read = read(fd, buffer, BUFFER_SIZE);
        
        if (bytes_read > 0) {
            complete_data.append(buffer, bytes_read); 
            conn_info->update_activity();
            total_reads_++;
            
            const char* data_ptr = complete_data.data();
            size_t data_size = complete_data.size();
            
            const char* header_end_ptr = nullptr;
            for (size_t i = 0; i + 3 < data_size; ++i) {
                if (data_ptr[i] == '\r' && data_ptr[i+1] == '\n' && 
                    data_ptr[i+2] == '\r' && data_ptr[i+3] == '\n') {
                    header_end_ptr = data_ptr + i;
                    break;
                }
            }
            
            if (header_end_ptr) {
                size_t header_size = header_end_ptr - data_ptr;
                size_t body_start = header_size + 4;
                
                size_t content_length = 0;
                std::string_view headers_view(data_ptr, header_size);
                
                size_t cl_pos = headers_view.find("content-length:");
                if (cl_pos == std::string_view::npos) {
                    cl_pos = headers_view.find("Content-Length:");
                }
                
                if (cl_pos != std::string_view::npos) {
                    size_t value_start = cl_pos + 15;
                    while (value_start < headers_view.size() && 
                           std::isspace(headers_view[value_start])) {
                        value_start++;
                    }
                    
                    size_t value_end = value_start;
                    while (value_end < headers_view.size() && 
                           std::isdigit(headers_view[value_end])) {
                        value_end++;
                    }
                    
                    if (value_end > value_start) {
                        std::string_view cl_value = headers_view.substr(value_start, value_end - value_start);
                        std::from_chars(cl_value.data(), cl_value.data() + cl_value.size(), content_length);
                    }
                }
                
                size_t current_body_size = data_size - body_start;
                if (current_body_size >= content_length) {
                    callback(conn_info, complete_data);
                    return;
                }
            }
        } else if (bytes_read == 0) {
            conn_info->connected = false;
            io_thread.connections.erase(fd);
            io_thread.read_callbacks.erase(fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                std::cerr << "[ERROR] Read error on fd " << fd << ": " << strerror(errno) << std::endl;
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
    
    auto write_buffer = std::make_shared<WriteBuffer>();
    write_buffer->data = event.write_data;
    write_buffer->callback = event.write_callback;
    
    if (try_write_immediate(io_thread, conn_info->fd, write_buffer)) {
        total_writes_++;
        if (write_buffer->callback) {
            write_buffer->callback(conn_info, true);
        }
    } else {
        io_thread.write_buffers[conn_info->fd] = write_buffer;
        
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;  
        ev.data.fd = conn_info->fd;
        
        if (epoll_ctl(io_thread.epoll_fd, EPOLL_CTL_MOD, conn_info->fd, &ev) == -1) {
            std::cerr << " epoll_ctl MOD for EPOLLOUT failed on fd " << conn_info->fd 
                      << ": " << strerror(errno) << std::endl;
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
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return false;
            } else {
                std::cerr << "[ERROR] Write error on fd " << fd << ": " << strerror(errno) << std::endl;
                return false;
            }
        }
    }
    return true; 
}

void IOThreadPool::handle_write_ready(IOThread& io_thread, int fd) {
    auto buffer_it = io_thread.write_buffers.find(fd);
    if (buffer_it == io_thread.write_buffers.end()) {
        return; 
    }
    
    auto buffer = buffer_it->second;
    auto conn_it = io_thread.connections.find(fd);
    if (conn_it == io_thread.connections.end()) {
        io_thread.write_buffers.erase(buffer_it);
        return;
    }
    
    auto conn_info = conn_it->second;
    
    if (try_write_immediate(io_thread, fd, buffer)) {
        total_writes_++;
        io_thread.write_buffers.erase(buffer_it);
        
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(io_thread.epoll_fd, EPOLL_CTL_MOD, fd, &ev);
        
        if (buffer->callback) {
            buffer->callback(conn_info, true);
        }
    } else {
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
