#include "thread_pool.hpp"
#include <iostream>

namespace Gecko {

ThreadPool::ThreadPool(size_t thread_count, bool cooperative_mode, std::chrono::milliseconds default_time_slice) 
    : stop_(false), cooperative_mode_(cooperative_mode), default_time_slice_(default_time_slice) {
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) {
            thread_count = 4; /* Default to four threads */
        }
    }
    
    std::cout << "[THREAD] Creating thread pool, thread count: " << thread_count << std::endl;
    
    /* Spawn worker threads */
    for (size_t i = 0; i < thread_count; ++i) {
        threads_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                ScheduledTask coop_task;
                bool has_coop_task = false;

                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    
                    /* Wait for work or stop signal */
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty() || !coop_tasks_.empty();
                    });
                    
                    /* Exit when stopping and queues are empty */
                    if (stop_ && tasks_.empty() && coop_tasks_.empty()) {
                        return;
                    }
                    
                    if (cooperative_mode_ && !coop_tasks_.empty()) {
                        coop_task = coop_tasks_.top();
                        coop_tasks_.pop();
                        has_coop_task = true;
                    } else if (!tasks_.empty()) {
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    } else if (!coop_tasks_.empty()) {
                        coop_task = coop_tasks_.top();
                        coop_tasks_.pop();
                        has_coop_task = true;
                    }
                }
                
                if (has_coop_task) {
                    TaskContext ctx{std::chrono::steady_clock::now() + coop_task.time_slice};
                    bool completed = true;
                    try {
                        completed = coop_task.task(ctx);
                    } catch (const std::exception& e) {
                        std::cerr << "Thread pool task threw an exception: " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << "Thread pool task threw an unknown exception" << std::endl;
                    }

                    if (!completed && !stop_) {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        coop_tasks_.push(ScheduledTask{
                            coop_task.priority,
                            next_sequence_++,
                            coop_task.task,
                            coop_task.time_slice
                        });
                        condition_.notify_one();
                    }
                    continue;
                }

                /* Execute regular task */
                if (task) {
                    try {
                        task();
                    } catch (const std::exception& e) {
                    std::cerr << "Thread pool task threw an exception: " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << "Thread pool task threw an unknown exception" << std::endl;
                    }
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    
    /* Wake up all threads */
    condition_.notify_all();
    
    /* Join all threads */
    for (std::thread& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    std::cout << "[THREAD] Thread pool stopped" << std::endl;
}

void ThreadPool::enable_cooperative_mode(std::chrono::milliseconds default_slice) {
    cooperative_mode_ = true;
    if (default_slice.count() > 0) {
        default_time_slice_ = default_slice;
    }
}

} /* namespace Gecko */
