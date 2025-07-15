#ifndef IO_THREAD_POOL_HPP
#define IO_THREAD_POOL_HPP

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <sys/epoll.h>
#include <unistd.h>

namespace Gecko {

struct ConnectionInfo;

// IO任务类型
enum class IOTaskType {
    READ,       // 读取数据
    WRITE,      // 写入数据
    KEEPALIVE   // 保持连接活跃
};

// IO任务结构
struct IOTask {
    IOTaskType type;
    std::shared_ptr<ConnectionInfo> conn_info;
    std::string data;  // 用于WRITE任务
    std::function<void(std::shared_ptr<ConnectionInfo>, const std::string&)> read_callback;  // 读取完成后的回调
    std::function<void(std::shared_ptr<ConnectionInfo>, bool)> write_callback;  // 写入完成后的回调
    
    IOTask(IOTaskType t, std::shared_ptr<ConnectionInfo> conn) 
        : type(t), conn_info(conn) {}
        
    IOTask(IOTaskType t, std::shared_ptr<ConnectionInfo> conn, const std::string& d) 
        : type(t), conn_info(conn), data(d) {}
        
    IOTask(IOTaskType t, std::shared_ptr<ConnectionInfo> conn, const std::string& d,
           std::function<void(std::shared_ptr<ConnectionInfo>, bool)> cb) 
        : type(t), conn_info(conn), data(d), write_callback(cb) {}
        
    IOTask(IOTaskType t, std::shared_ptr<ConnectionInfo> conn, 
           std::function<void(std::shared_ptr<ConnectionInfo>, const std::string&)> cb) 
        : type(t), conn_info(conn), read_callback(cb) {}
};

// 专门的IO线程池
class IOThreadPool {
public:
    explicit IOThreadPool(size_t io_thread_count = 2);
    ~IOThreadPool();

    // 禁止复制和移动
    IOThreadPool(const IOThreadPool&) = delete;
    IOThreadPool& operator=(const IOThreadPool&) = delete;
    IOThreadPool(IOThreadPool&&) = delete;
    IOThreadPool& operator=(IOThreadPool&&) = delete;

    // 异步读取数据
    void async_read(std::shared_ptr<ConnectionInfo> conn_info, 
                   std::function<void(std::shared_ptr<ConnectionInfo>, const std::string&)> callback);
    
    // 异步写入数据
    void async_write(std::shared_ptr<ConnectionInfo> conn_info, const std::string& data);
    
    // 异步写入数据（带完成回调）
    void async_write(std::shared_ptr<ConnectionInfo> conn_info, const std::string& data, 
                    std::function<void(std::shared_ptr<ConnectionInfo>, bool)> callback);
    
    // 保持连接活跃
    void keep_alive(std::shared_ptr<ConnectionInfo> conn_info);
    
    // 关闭连接
    void close_connection(std::shared_ptr<ConnectionInfo> conn_info);
    
    // 获取状态
    size_t thread_count() const { return threads_.size(); }
    size_t pending_tasks() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return task_queue_.size();
    }
    
    // 停止IO线程池
    void stop();

private:
    void io_worker_thread();
    void process_read_task(const IOTask& task);
    void process_write_task(const IOTask& task);
    void process_keepalive_task(const IOTask& task);
    
    std::vector<std::thread> threads_;
    std::queue<IOTask> task_queue_;
    
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_flag_{false};
    
    // 每个IO线程的epoll fd
    std::vector<int> epoll_fds_;
    
    // 统计信息
    std::atomic<size_t> total_reads_{0};
    std::atomic<size_t> total_writes_{0};
    std::atomic<size_t> total_connections_{0};
};

} // namespace Gecko

#endif 