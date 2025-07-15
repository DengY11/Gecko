#include "io_thread_pool.hpp"
#include "server.hpp"
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

namespace Gecko {

IOThreadPool::IOThreadPool(size_t io_thread_count) : stop_flag_(false) {
    if (io_thread_count == 0) {
        io_thread_count = 2; // 默认2个IO线程
    }
    
    std::cout << "🔄 创建IO线程池，IO线程数量: " << io_thread_count << std::endl;
    
    // 为每个IO线程创建epoll fd
    epoll_fds_.resize(io_thread_count);
    for (size_t i = 0; i < io_thread_count; ++i) {
        epoll_fds_[i] = epoll_create1(0);
        if (epoll_fds_[i] == -1) {
            throw std::runtime_error("Failed to create epoll fd for IO thread: " + std::string(strerror(errno)));
        }
    }
    
    // 创建IO工作线程
    for (size_t i = 0; i < io_thread_count; ++i) {
        threads_.emplace_back([this] {
            io_worker_thread();
        });
    }
}

IOThreadPool::~IOThreadPool() {
    stop();
    
    // 关闭epoll文件描述符
    for (int epoll_fd : epoll_fds_) {
        if (epoll_fd != -1) {
            close(epoll_fd);
        }
    }
    
    std::cout << "🔄 IO线程池已关闭" << std::endl;
}

void IOThreadPool::stop() {
    stop_flag_ = true;
    condition_.notify_all();
    
    for (std::thread& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    threads_.clear();
}

void IOThreadPool::async_read(std::shared_ptr<ConnectionInfo> conn_info, 
                             std::function<void(std::shared_ptr<ConnectionInfo>, const std::string&)> callback) {
    if (stop_flag_) {
        return;
    }
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        task_queue_.emplace(IOTaskType::READ, conn_info, callback);
    }
    condition_.notify_one();
}

void IOThreadPool::async_write(std::shared_ptr<ConnectionInfo> conn_info, const std::string& data) {
    async_write(conn_info, data, nullptr);
}

void IOThreadPool::async_write(std::shared_ptr<ConnectionInfo> conn_info, const std::string& data, 
                              std::function<void(std::shared_ptr<ConnectionInfo>, bool)> callback) {
    if (stop_flag_) {
        return;
    }
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (callback) {
            task_queue_.emplace(IOTaskType::WRITE, conn_info, data, callback);
        } else {
            task_queue_.emplace(IOTaskType::WRITE, conn_info, data);
        }
    }
    condition_.notify_one();
}

void IOThreadPool::keep_alive(std::shared_ptr<ConnectionInfo> conn_info) {
    if (stop_flag_) {
        return;
    }
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        task_queue_.emplace(IOTaskType::KEEPALIVE, conn_info);
    }
    condition_.notify_one();
}

void IOThreadPool::close_connection(std::shared_ptr<ConnectionInfo> conn_info) {
    if (conn_info && conn_info->connected) {
        conn_info->connected = false;
        close(conn_info->fd);
    }
}

void IOThreadPool::io_worker_thread() {
    while (!stop_flag_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // 等待任务
        condition_.wait(lock, [this] {
            return stop_flag_ || !task_queue_.empty();
        });
        
        if (stop_flag_ && task_queue_.empty()) {
            break;
        }
        
        // 处理任务
        while (!task_queue_.empty()) {
            IOTask task = std::move(task_queue_.front());
            task_queue_.pop();
            lock.unlock();
            
            // 处理不同类型的IO任务
            try {
                switch (task.type) {
                    case IOTaskType::READ:
                        process_read_task(task);
                        break;
                    case IOTaskType::WRITE:
                        process_write_task(task);
                        break;
                    case IOTaskType::KEEPALIVE:
                        process_keepalive_task(task);
                        break;
                }
            } catch (const std::exception& e) {
                std::cerr << "❌ IO线程任务处理异常: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "❌ IO线程任务处理未知异常" << std::endl;
            }
            
            lock.lock();
        }
    }
}

void IOThreadPool::process_read_task(const IOTask& task) {
    auto conn_info = task.conn_info;
    if (!conn_info || !conn_info->connected) {
        std::cout << "❌ [IO线程] 读取任务失败：连接无效或已关闭" << std::endl;
        return;
    }
    
    std::cout << "📥 [IO线程] 开始读取数据，fd: " << conn_info->fd << std::endl;
    
    const size_t BUFFER_SIZE = 8192;
    char buffer[BUFFER_SIZE];
    std::string complete_data;
    
    // 持续读取直到请求完整或连接关闭
    while (conn_info->connected) {
        ssize_t bytes_read = read(conn_info->fd, buffer, BUFFER_SIZE - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            complete_data += buffer;
            conn_info->update_activity();
            
            std::cout << "📥 [IO线程] 读取了 " << bytes_read << " 字节，总共: " << complete_data.length() << " 字节" << std::endl;
            
            // 检查是否已经读取完整请求
            if (complete_data.find("\r\n\r\n") != std::string::npos) {
                std::cout << "✅ [IO线程] 检测到请求头结束标记" << std::endl;
                
                // 检查是否有Content-Length，如果有则继续读取body
                size_t header_end = complete_data.find("\r\n\r\n");
                std::string headers = complete_data.substr(0, header_end);
                
                size_t content_length = 0;
                size_t pos = headers.find("Content-Length:");
                if (pos != std::string::npos) {
                    pos += 15; // strlen("Content-Length:")
                    while (pos < headers.length() && std::isspace(headers[pos])) pos++;
                    size_t end_pos = pos;
                    while (end_pos < headers.length() && std::isdigit(headers[end_pos])) end_pos++;
                    if (end_pos > pos) {
                        content_length = std::stoul(headers.substr(pos, end_pos - pos));
                        std::cout << "📏 [IO线程] 检测到Content-Length: " << content_length << std::endl;
                    }
                }
                
                size_t current_body_size = complete_data.length() - header_end - 4;
                std::cout << "📏 [IO线程] 当前body大小: " << current_body_size << "，期望: " << content_length << std::endl;
                
                if (current_body_size >= content_length) {
                    // 请求完整，调用回调
                    std::cout << "✅ [IO线程] 请求完整，准备调用回调" << std::endl;
                    break;
                }
            }
            
            total_reads_++;
        } else if (bytes_read == 0) {
            // 连接关闭
            std::cout << "❌ [IO线程] 连接关闭，fd: " << conn_info->fd << std::endl;
            conn_info->connected = false;
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 暂无数据可读，继续等待
                std::cout << "⏸️ [IO线程] 暂无数据可读，继续等待，fd: " << conn_info->fd << std::endl;
                continue;
            } else {
                // 读取错误
                std::cerr << "❌ [IO线程] 读取错误 " << conn_info->peer_addr 
                          << ": " << strerror(errno) << std::endl;
                conn_info->connected = false;
                break;
            }
        }
    }
    
    // 如果读取到完整数据，调用回调
    if (!complete_data.empty() && task.read_callback) {
        std::cout << "📞 [IO线程] 调用回调函数，数据长度: " << complete_data.length() << std::endl;
        task.read_callback(conn_info, complete_data);
    } else {
        std::cout << "❌ [IO线程] 无法调用回调：数据为空或回调函数为空" << std::endl;
    }
}

void IOThreadPool::process_write_task(const IOTask& task) {
    auto conn_info = task.conn_info;
    if (!conn_info || !conn_info->connected) {
        std::cout << "❌ [IO线程] 写入任务失败：连接无效或已关闭" << std::endl;
        if (task.write_callback) {
            task.write_callback(conn_info, false);
        }
        return;
    }
    
    std::cout << "📤 [IO线程] 开始写入数据，fd: " << conn_info->fd << "，数据长度: " << task.data.length() << std::endl;
    
    const std::string& data = task.data;
    size_t total_sent = 0;
    
    while (total_sent < data.length() && conn_info->connected) {
        ssize_t bytes_sent = write(conn_info->fd, data.c_str() + total_sent, 
                                  data.length() - total_sent);
        
        if (bytes_sent > 0) {
            total_sent += bytes_sent;
            conn_info->update_activity();
            std::cout << "📤 [IO线程] 写入了 " << bytes_sent << " 字节，总共 " << total_sent 
                      << "/" << data.length() << " 字节" << std::endl;
        } else if (bytes_sent == 0) {
            // 连接关闭
            std::cout << "❌ [IO线程] 连接关闭，fd: " << conn_info->fd << std::endl;
            conn_info->connected = false;
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 暂时无法写入，稍后重试
                std::cout << "⏸️ [IO线程] 写入阻塞，等待重试，fd: " << conn_info->fd << std::endl;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            } else {
                // 写入错误
                std::cerr << "❌ [IO线程] 写入错误 " << conn_info->peer_addr 
                          << ": " << strerror(errno) << std::endl;
                conn_info->connected = false;
                break;
            }
        }
    }
    
    bool success = (total_sent == data.length());
    if (success) {
        std::cout << "✅ [IO线程] 写入完成，fd: " << conn_info->fd << std::endl;
        total_writes_++;
    } else {
        std::cout << "❌ [IO线程] 写入失败，fd: " << conn_info->fd << "，已写入: " << total_sent 
                  << "/" << data.length() << " 字节" << std::endl;
    }
    
    // 调用写入完成回调
    if (task.write_callback) {
        std::cout << "📞 [IO线程] 调用写入完成回调，成功: " << (success ? "是" : "否") << std::endl;
        task.write_callback(conn_info, success);
    }
}

void IOThreadPool::process_keepalive_task(const IOTask& task) {
    auto conn_info = task.conn_info;
    if (!conn_info || !conn_info->connected) {
        return;
    }
    
    // 更新活动时间，保持连接
    conn_info->update_activity();
    
    // 可以在这里添加心跳检测逻辑
    #ifdef DEBUG
    std::cout << "💓 保持连接活跃: " << conn_info->peer_addr << std::endl;
    #endif
}

} // namespace Gecko 