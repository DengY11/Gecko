#include "thread_pool.hpp"
#include <iostream>

namespace Gecko {

ThreadPool::ThreadPool(size_t thread_count) : stop_(false) {
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
                
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    
                    /* Wait for work or stop signal */
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });
                    
                    /* Exit when stopping and queue is empty */
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    
                    /* Pull next task */
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                
                /* Execute task */
                try {
                    task();
                } catch (const std::exception& e) {
                std::cerr << "Thread pool task threw an exception: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "Thread pool task threw an unknown exception" << std::endl;
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

} /* namespace Gecko */
