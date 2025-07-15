#ifndef REQUEST_POOL_HPP
#define REQUEST_POOL_HPP

#include "fast_http_parser.hpp"
#include "http_request.hpp"
#include <memory>
#include <stack>
#include <cstring>
#include <mutex>
#include <atomic>
#include <vector>

namespace Gecko {

// 高性能的请求缓冲区
class RequestBuffer {
public:
    static constexpr size_t DEFAULT_SIZE = 16384; // 16KB
    
    RequestBuffer(size_t size = DEFAULT_SIZE) : capacity_(size), size_(0) {
        data_ = std::make_unique<char[]>(capacity_);
    }
    
    void reset() {
        size_ = 0;
    }
    
    char* data() { return data_.get(); }
    const char* data() const { return data_.get(); }
    size_t capacity() const { return capacity_; }
    size_t size() const { return size_; }
    
    void set_size(size_t new_size) {
        if (new_size <= capacity_) {
            size_ = new_size;
        }
    }
    
    bool append(const char* src, size_t len) {
        if (size_ + len > capacity_) {
            return false; // 缓冲区不足
        }
        std::memcpy(data_.get() + size_, src, len);
        size_ += len;
        return true;
    }
    
    std::string_view view() const {
        return std::string_view(data_.get(), size_);
    }

private:
    std::unique_ptr<char[]> data_;
    size_t capacity_;
    size_t size_;
};

// 池化的请求对象
class PooledRequest {
public:
    PooledRequest() : buffer_(std::make_unique<RequestBuffer>()) {}
    
    void reset() {
        buffer_->reset();
        fast_request_.reset();
        http_request_valid_ = false;
    }
    
    RequestBuffer* get_buffer() { return buffer_.get(); }
    FastHttpRequest& get_fast_request() { return fast_request_; }
    
    // 懒加载的HttpRequest转换
    HttpRequest& get_http_request() {
        if (!http_request_valid_) {
            HttpRequestAdapter::convert(fast_request_, http_request_);
            http_request_valid_ = true;
        }
        return http_request_;
    }
    
    bool parse() {
        return FastHttpParser::parse(buffer_->view(), fast_request_);
    }

private:
    std::unique_ptr<RequestBuffer> buffer_;
    FastHttpRequest fast_request_;
    HttpRequest http_request_;
    bool http_request_valid_ = false;
};

// 高性能请求池
class RequestPool {
public:
    static constexpr size_t DEFAULT_POOL_SIZE = 2048; // 预分配2048个请求对象
    
    explicit RequestPool(size_t pool_size = DEFAULT_POOL_SIZE) 
        : pool_size_(pool_size), allocated_count_(0) {
        
        // 预分配所有请求对象
        for (size_t i = 0; i < pool_size; ++i) {
            available_requests_.push(std::make_unique<PooledRequest>());
        }
    }
    
    std::unique_ptr<PooledRequest> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::unique_ptr<PooledRequest> request;
        
        if (!available_requests_.empty()) {
            request = std::move(available_requests_.top());
            available_requests_.pop();
            request->reset();
        } else {
            // 池已空，创建新对象
            request = std::make_unique<PooledRequest>();
            pool_misses_++;
        }
        
        allocated_count_++;
        return request;
    }
    
    void release(std::unique_ptr<PooledRequest> request) {
        if (!request) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        allocated_count_--;
        
        if (available_requests_.size() < pool_size_) {
            request->reset();
            available_requests_.push(std::move(request));
        }
        // 如果池已满，直接丢弃对象
    }
    
    // 统计信息
    size_t available_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_requests_.size();
    }
    
    size_t allocated_count() const {
        return allocated_count_.load();
    }
    
    size_t pool_misses() const {
        return pool_misses_.load();
    }
    
    double hit_rate() const {
        size_t total = allocated_count_.load() + available_count();
        return total > 0 ? (1.0 - static_cast<double>(pool_misses_.load()) / total) : 1.0;
    }

private:
    size_t pool_size_;
    std::atomic<size_t> allocated_count_;
    std::atomic<size_t> pool_misses_{0};
    
    mutable std::mutex mutex_;
    std::stack<std::unique_ptr<PooledRequest>> available_requests_;
};

// RAII请求管理器
class RequestManager {
public:
    RequestManager(std::shared_ptr<RequestPool> pool) 
        : pool_(pool), request_(pool_->acquire()) {}
    
    ~RequestManager() {
        if (request_ && pool_) {
            pool_->release(std::move(request_));
        }
    }
    
    // 禁止拷贝，允许移动
    RequestManager(const RequestManager&) = delete;
    RequestManager& operator=(const RequestManager&) = delete;
    
    RequestManager(RequestManager&& other) noexcept
        : pool_(std::move(other.pool_)), request_(std::move(other.request_)) {}
    
    RequestManager& operator=(RequestManager&& other) noexcept {
        if (this != &other) {
            if (request_ && pool_) {
                pool_->release(std::move(request_));
            }
            pool_ = std::move(other.pool_);
            request_ = std::move(other.request_);
        }
        return *this;
    }
    
    PooledRequest* get() { return request_.get(); }
    PooledRequest* operator->() { return request_.get(); }
    PooledRequest& operator*() { return *request_; }

private:
    std::shared_ptr<RequestPool> pool_;
    std::unique_ptr<PooledRequest> request_;
};

} // namespace Gecko

#endif // REQUEST_POOL_HPP 