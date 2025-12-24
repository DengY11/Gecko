#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <iostream>

namespace Gecko {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    FATAL = 4,
    OFF = 5
};

enum class LogOutput {
    CONSOLE,    
    FILE,       
    BOTH        
};

inline std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::OFF:   return "OFF";
        default: return "UNKNOWN";
    }
}

class Logger {
public:
    Logger();
    
    Logger(LogLevel level, size_t num_threads = 1);
    
    Logger(LogLevel level, size_t num_threads, LogOutput output, const std::string& filename = "log.txt");
    
    ~Logger();

    void log(LogLevel level, const std::string &message);
    void set_log_level(LogLevel level);
    void set_output(LogOutput output, const std::string& filename = "log.txt");

private:
    void log_worker_func();
    void write_to_log(const std::string &message);
    void init_threads();
    void cleanup_threads();
    
    size_t num_threads;
    std::vector<std::thread> threads;
    std::queue<std::string> log_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> running;
    LogLevel log_level;
    LogOutput output_target;
    std::ofstream log_file;
    std::string filename;
};

} // namespace Gecko
#endif
