#include "thread_pool.hpp"
#include <iostream>

namespace Gecko {

ThreadPool::ThreadPool(size_t thread_count) : stop_(false) {
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) {
            thread_count = 4; // 默认4个线程
        }
    }
    
    std::cout << "🧵 创建线程池，线程数量: " << thread_count << std::endl;
    
    // 创建工作线程
    for (size_t i = 0; i < thread_count; ++i) {
        threads_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    
                    // 等待任务或停止信号
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });
                    
                    // 如果收到停止信号且任务队列为空，退出
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    
                    // 获取任务
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                
                // 执行任务
                try {
                    task();
                } catch (const std::exception& e) {
                    std::cerr << "线程池任务执行异常: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "线程池任务执行未知异常" << std::endl;
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
    
    // 通知所有线程
    condition_.notify_all();
    
    // 等待所有线程完成
    for (std::thread& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    std::cout << "🧵 线程池已关闭" << std::endl;
}

} // namespace Gecko 