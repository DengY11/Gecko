#include "logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Gecko {

// 默认构造函数：INFO级别，单线程，输出到终端
Logger::Logger() : Logger(LogLevel::INFO, 1, LogOutput::CONSOLE) {
}

// 指定级别和线程数，默认输出到终端
Logger::Logger(LogLevel level, size_t num_threads) 
    : Logger(level, num_threads, LogOutput::CONSOLE) {
}

// 完整构造函数
Logger::Logger(LogLevel level, size_t num_threads, LogOutput output, const std::string& filename)
    : num_threads(num_threads), running(true), log_level(level), 
      output_target(output), filename(filename) {
    
    if (output_target == LogOutput::FILE || output_target == LogOutput::BOTH) {
        log_file.open(filename, std::ios::app);
        if (!log_file.is_open()) {
            std::cerr << "Warning: Failed to open log file: " << filename << std::endl;
        }
    }
    
    init_threads();
}

Logger::~Logger() {
    cleanup_threads();
    if (log_file.is_open()) {
        log_file.close();
    }
}

void Logger::init_threads() {
    for (size_t i = 0; i < num_threads; i++) {
        threads.push_back(std::thread(&Logger::log_worker_func, this));
    }
}

void Logger::cleanup_threads() {
    running = false;
    cv.notify_all();
    for (auto &thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads.clear();
}

// 添加run()方法
void Logger::run() {
    // 这个方法用于手动启动logger，在构造函数中已经启动了worker线程
    // 这里可以用于重启或者其他控制逻辑
}

// 设置日志级别
void Logger::set_log_level(LogLevel level) {
    log_level = level;
}

// 设置输出目标
void Logger::set_output(LogOutput output, const std::string& new_filename) {
    output_target = output;
    
    // 如果需要文件输出且文件名发生变化，重新打开文件
    if ((output == LogOutput::FILE || output == LogOutput::BOTH) && 
        new_filename != filename) {
        
        if (log_file.is_open()) {
            log_file.close();
        }
        
        filename = new_filename;
        log_file.open(filename, std::ios::app);
        if (!log_file.is_open()) {
            std::cerr << "Warning: Failed to open log file: " << filename << std::endl;
        }
    }
}

void Logger::log(LogLevel level, const std::string &message) {
    if (level < log_level) {
        return;
    }
    
    // 格式化日志消息，添加时间戳和日志级别
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
    ss << "[" << logLevelToString(level) << "] " << message;
    
    std::string logMessage = ss.str();
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        log_queue.push(logMessage);
    }
    cv.notify_one();
}

void Logger::log_worker_func() {
    while (running) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        cv.wait(lock, [this] { return !log_queue.empty() || !running; });
        
        while (!log_queue.empty()) {
            const std::string message = log_queue.front();
            log_queue.pop();
            lock.unlock();
            
            write_to_log(message);
            
            lock.lock();
        }
    }
}

void Logger::write_to_log(const std::string &message) {
    // 根据输出目标选择输出方式
    switch (output_target) {
        case LogOutput::CONSOLE:
            std::cout << message << std::endl;
            break;
            
        case LogOutput::FILE:
            if (log_file.is_open()) {
                log_file << message << std::endl;
                log_file.flush(); // 立即刷新到文件
            }
            break;
            
        case LogOutput::BOTH:
            std::cout << message << std::endl;
            if (log_file.is_open()) {
                log_file << message << std::endl;
                log_file.flush();
            }
            break;
    }
}

} // namespace Gecko
