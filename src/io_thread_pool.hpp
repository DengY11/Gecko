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

// IO操作类型
enum class IOOperation {
    READ,
    WRITE
};

// IO任务结构 - 简化版
struct IOEvent {
    int fd;
    IOOperation operation;
    std::shared_ptr<ConnectionInfo> conn_info;
    std::string write_data;  // 用于写操作
    std::function<void(std::shared_ptr<ConnectionInfo>, const std::string&)> read_callback;
    std::function<void(std::shared_ptr<ConnectionInfo>, bool)> write_callback;
};

// 专门的异步IO线程池 - Reactor模式
class IOThreadPool {
public:
    explicit IOThreadPool(size_t io_thread_count = 2);
    ~IOThreadPool();

    // 禁止复制和移动
    IOThreadPool(const IOThreadPool&) = delete;
    IOThreadPool& operator=(const IOThreadPool&) = delete;
    IOThreadPool(IOThreadPool&&) = delete;
    IOThreadPool& operator=(IOThreadPool&&) = delete;

    // 注册连接到IO线程进行读取监听
    void register_read(std::shared_ptr<ConnectionInfo> conn_info, 
                      std::function<void(std::shared_ptr<ConnectionInfo>, const std::string&)> callback);
    
    // 异步写入数据
    void async_write(std::shared_ptr<ConnectionInfo> conn_info, const std::string& data);
    
    // 异步写入数据（带完成回调）
    void async_write(std::shared_ptr<ConnectionInfo> conn_info, const std::string& data, 
                    std::function<void(std::shared_ptr<ConnectionInfo>, bool)> callback);
    
    // 取消注册连接
    void unregister_connection(std::shared_ptr<ConnectionInfo> conn_info);
    
    // 获取状态
    size_t thread_count() const { return io_threads_.size(); }
    
    // 停止IO线程池
    void stop();

private:
    // 写入缓冲区结构
    struct WriteBuffer {
        std::string data;
        size_t offset = 0;
        std::function<void(std::shared_ptr<ConnectionInfo>, bool)> callback;
        
        bool is_complete() const { return offset >= data.length(); }
        std::string_view remaining() const { return std::string_view(data).substr(offset); }
    };

    // IO线程数据结构
    struct IOThread {
        std::thread thread;
        int epoll_fd;
        int wakeup_fd[2];  // 用于唤醒epoll的管道
        std::mutex events_mutex;
        std::queue<IOEvent> pending_events;
        std::unordered_map<int, std::shared_ptr<ConnectionInfo>> connections;
        std::unordered_map<int, std::function<void(std::shared_ptr<ConnectionInfo>, const std::string&)>> read_callbacks;
        std::unordered_map<int, std::shared_ptr<WriteBuffer>> write_buffers; // 写入缓冲区
        std::atomic<bool> running{true};
        
        IOThread() : epoll_fd(-1) {
            wakeup_fd[0] = wakeup_fd[1] = -1;
        }
        
        ~IOThread() {
            if (epoll_fd != -1) close(epoll_fd);
            if (wakeup_fd[0] != -1) close(wakeup_fd[0]);
            if (wakeup_fd[1] != -1) close(wakeup_fd[1]);
        }
    };
    
    void io_reactor_loop(IOThread& io_thread);
    void process_pending_events(IOThread& io_thread);
    void handle_read_event(IOThread& io_thread, int fd);
    void handle_write_event(IOThread& io_thread, const IOEvent& event);
    void handle_write_ready(IOThread& io_thread, int fd);
    bool try_write_immediate(IOThread& io_thread, int fd, std::shared_ptr<WriteBuffer> buffer);
    void wakeup_thread(IOThread& io_thread);
    int get_next_thread_index();
    
    std::vector<std::unique_ptr<IOThread>> io_threads_;
    std::atomic<bool> stop_flag_{false};
    std::atomic<size_t> round_robin_index_{0};
    
    // 统计信息
    std::atomic<size_t> total_reads_{0};
    std::atomic<size_t> total_writes_{0};
};

} // namespace Gecko

#endif 