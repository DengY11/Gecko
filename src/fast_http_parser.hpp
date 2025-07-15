#ifndef FAST_HTTP_PARSER_HPP
#define FAST_HTTP_PARSER_HPP

#include <string_view>
#include <unordered_map>
#include <string>
#include <map>
#include <algorithm>
#include <cctype>

namespace Gecko {

enum class FastHttpMethod { GET, POST, HEAD, PUT, DELETE, UNKNOWN };

// 快速字符串视图比较器（大小写不敏感）
struct FastCaseInsensitiveHash {
    size_t operator()(const std::string_view& str) const {
        size_t hash = 0;
        for (char c : str) {
            hash ^= std::hash<char>{}(std::tolower(c)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

struct FastCaseInsensitiveEqual {
    bool operator()(const std::string_view& lhs, const std::string_view& rhs) const {
        if (lhs.size() != rhs.size()) return false;
        return std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                         [](char a, char b) { return std::tolower(a) == std::tolower(b); });
    }
};

// 使用string_view的快速头部映射
using FastHeaderMap = std::unordered_map<std::string_view, std::string_view, 
                                        FastCaseInsensitiveHash, FastCaseInsensitiveEqual>;

// 快速HTTP请求结构（零拷贝）
struct FastHttpRequest {
    FastHttpMethod method = FastHttpMethod::UNKNOWN;
    std::string_view url;
    std::string_view version;
    std::string_view body;
    FastHeaderMap headers;
    std::unordered_map<std::string_view, std::string_view> query_params;
    
    // 原始数据引用（保持数据有效性）
    const char* raw_data = nullptr;
    size_t raw_size = 0;
    
    void reset() {
        method = FastHttpMethod::UNKNOWN;
        url = {};
        version = {};
        body = {};
        headers.clear();
        query_params.clear();
        raw_data = nullptr;
        raw_size = 0;
    }
};

// 高性能HTTP解析器类
class FastHttpParser {
public:
    // 解析HTTP请求（零拷贝）
    static bool parse(const char* data, size_t size, FastHttpRequest& request);
    static bool parse(std::string_view data, FastHttpRequest& request);
    
private:
    // 内联工具函数，优化性能
    static inline std::string_view trim(std::string_view str) {
        size_t start = 0;
        while (start < str.size() && std::isspace(str[start])) ++start;
        size_t end = str.size();
        while (end > start && std::isspace(str[end - 1])) --end;
        return str.substr(start, end - start);
    }
    
    static inline FastHttpMethod parse_method(std::string_view method_str) {
        // 使用字符数组比较，比字符串比较更快
        if (method_str.size() == 3) {
            if (method_str[0] == 'G' && method_str[1] == 'E' && method_str[2] == 'T') 
                return FastHttpMethod::GET;
            if (method_str[0] == 'P' && method_str[1] == 'U' && method_str[2] == 'T') 
                return FastHttpMethod::PUT;
        } else if (method_str.size() == 4) {
            if (method_str[0] == 'P' && method_str[1] == 'O' && method_str[2] == 'S' && method_str[3] == 'T') 
                return FastHttpMethod::POST;
            if (method_str[0] == 'H' && method_str[1] == 'E' && method_str[2] == 'A' && method_str[3] == 'D') 
                return FastHttpMethod::HEAD;
        } else if (method_str.size() == 6) {
            if (method_str.substr(0, 6) == "DELETE") 
                return FastHttpMethod::DELETE;
        }
        return FastHttpMethod::UNKNOWN;
    }
    
    static inline const char* find_crlf(const char* start, const char* end) {
        // 优化的CRLF查找
        const char* pos = start;
        while (pos + 1 < end) {
            if (pos[0] == '\r' && pos[1] == '\n') {
                return pos;
            }
            ++pos;
        }
        return nullptr;
    }
    
    static inline const char* find_double_crlf(const char* start, const char* end) {
        // 优化的双CRLF查找
        const char* pos = start;
        while (pos + 3 < end) {
            if (pos[0] == '\r' && pos[1] == '\n' && pos[2] == '\r' && pos[3] == '\n') {
                return pos;
            }
            ++pos;
        }
        return nullptr;
    }
    
    static bool parse_request_line(std::string_view line, FastHttpRequest& request);
    static bool parse_headers(std::string_view headers_block, FastHttpRequest& request);
    static void parse_query_params(std::string_view url, FastHttpRequest& request);
    static size_t parse_content_length(const FastHeaderMap& headers);
};

// 将FastHttpRequest转换为原始HttpRequest的适配器
class HttpRequestAdapter {
public:
    static void convert(const FastHttpRequest& fast_req, class HttpRequest& request);
};

} // namespace Gecko

#endif // FAST_HTTP_PARSER_HPP 