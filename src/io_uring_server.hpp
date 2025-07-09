#ifndef IO_URING_SERVER_HPP
#define IO_URING_SERVER_HPP

#include <liburing.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace Gecko {

// 前置声明
class Context;
using RequestHandler = std::function<void(Context&)>;

// I/O操作类型
enum class IoOpType {
    ACCEPT = 1,
    READ = 2,
    WRITE = 3
};

// I/O请求结构
struct IoRequest {
    IoOpType type;
    int fd;
    char* buffer;
    size_t size;
    std::string response_data; // 用于写操作
    
    IoRequest(IoOpType t, int f) : type(t), fd(f), buffer(nullptr), size(0) {}
};

class IoUringServer {
public:
    explicit IoUringServer(int port, size_t queue_depth = 256);
    ~IoUringServer();
    
    void run(RequestHandler handler);
    void stop();

private:
    // io_uring 相关
    struct io_uring ring_;
    size_t queue_depth_;
    
    // 服务器状态
    int port_;
    int listen_fd_;
    bool running_;
    RequestHandler request_handler_;
    
    // 缓冲区管理
    static constexpr size_t BUFFER_SIZE = 8192;
    std::vector<char> buffer_pool_;
    
    // 请求管理
    std::unordered_map<uint64_t, std::unique_ptr<IoRequest>> pending_requests_;
    uint64_t next_request_id_;
    
    // 初始化和清理
    void setup_io_uring();
    void setup_listen_socket();
    void cleanup();
    
    // I/O操作提交
    void submit_accept();
    void submit_read(int client_fd);
    void submit_write(int client_fd, const std::string& data);
    
    // 事件处理
    void process_completions();
    void handle_accept_completion(struct io_uring_cqe* cqe, IoRequest* req);
    void handle_read_completion(struct io_uring_cqe* cqe, IoRequest* req);
    void handle_write_completion(struct io_uring_cqe* cqe, IoRequest* req);
    
    // 辅助函数
    char* get_buffer();
    void return_buffer(char* buffer);
    uint64_t generate_request_id();
};

} // namespace Gecko

#endif 