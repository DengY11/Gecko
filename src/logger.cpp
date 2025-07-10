#include "logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Gecko {

Logger::Logger() : Logger(LogLevel::INFO, 1, LogOutput::CONSOLE) {
}

Logger::Logger(LogLevel level, size_t num_threads) 
    : Logger(level, num_threads, LogOutput::CONSOLE) {
}

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

void Logger::set_log_level(LogLevel level) {
    log_level = level;
}

void Logger::set_output(LogOutput output, const std::string& new_filename) {
    output_target = output;
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
    switch (output_target) {
        case LogOutput::CONSOLE:
            std::cout << message << std::endl;
            break;
            
        case LogOutput::FILE:
            if (log_file.is_open()) {
                log_file << message << std::endl;
                log_file.flush();
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
