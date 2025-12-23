#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace Gecko {

class ThreadPool {
public:
    enum class TaskPriority { LOW = 0, NORMAL = 1, HIGH = 2 };

    struct TaskContext {
        std::chrono::steady_clock::time_point deadline;
        bool should_yield() const {
            return std::chrono::steady_clock::now() >= deadline;
        }
    };

    using CooperativeTask = std::function<bool(TaskContext&)>;

    ThreadPool(size_t thread_count = std::thread::hardware_concurrency(),
               bool cooperative_mode = false,
               std::chrono::milliseconds default_time_slice = std::chrono::milliseconds(2));
    ~ThreadPool();

    /* Non-copyable, non-movable */
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /* Enqueue task into pool */
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type> {
        
        using return_type = typename std::result_of<F(Args...)>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            
            tasks_.emplace([task]() {
                (*task)();
            });
        }
        
        condition_.notify_one();
        return result;
    }

    /* Enqueue cooperative task with optional priority and time slice */
    template<typename F>
    void enqueue_cooperative(F&& task,
                             TaskPriority priority = TaskPriority::NORMAL,
                             std::chrono::milliseconds time_slice = std::chrono::milliseconds(0)) {
        std::chrono::milliseconds slice = (time_slice.count() > 0) ? time_slice : default_time_slice_;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            coop_tasks_.push(ScheduledTask{
                static_cast<int>(priority),
                next_sequence_++,
                CooperativeTask(std::forward<F>(task)),
                slice
            });
        }
        condition_.notify_one();
    }

    void enable_cooperative_mode(std::chrono::milliseconds default_slice);

    /* Inspect pool state */
    size_t thread_count() const { return threads_.size(); }
    size_t pending_tasks() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    struct ScheduledTask {
        int priority;
        std::uint64_t sequence;
        CooperativeTask task;
        std::chrono::milliseconds time_slice;
    };
    struct TaskComparator {
        bool operator()(const ScheduledTask& lhs, const ScheduledTask& rhs) const {
            if (lhs.priority == rhs.priority) {
                return lhs.sequence > rhs.sequence; /* FIFO for equal priority */
            }
            return lhs.priority < rhs.priority; /* Higher priority first */
        }
    };
    std::priority_queue<ScheduledTask, std::vector<ScheduledTask>, TaskComparator> coop_tasks_;
    
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
    bool cooperative_mode_{false};
    std::chrono::milliseconds default_time_slice_{2};
    std::uint64_t next_sequence_{0};
};

} /* namespace Gecko */

#endif 
