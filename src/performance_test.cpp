#include "fast_http_parser.hpp"
#include "http_request.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <string>

using namespace Gecko;
using namespace std::chrono;

// 测试用的HTTP请求
const std::string test_requests[] = {
    "GET /api/users?page=1&limit=10 HTTP/1.1\r\n"
    "Host: api.example.com\r\n"
    "User-Agent: TestClient/1.0\r\n"
    "Accept: application/json\r\n"
    "Connection: keep-alive\r\n"
    "\r\n",
    
    "POST /api/login HTTP/1.1\r\n"
    "Host: auth.example.com\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 43\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "{\"username\":\"admin\",\"password\":\"secret123\"}",
    
    "PUT /api/users/123 HTTP/1.1\r\n"
    "Host: api.example.com\r\n"
    "Content-Type: application/x-www-form-urlencoded\r\n"
    "Content-Length: 24\r\n"
    "Authorization: Bearer token123456789\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "name=John&email=john@doe"
};

void test_original_parser_performance() {
    std::cout << "=== 测试原始解析器性能 ===" << std::endl;
    
    const int iterations = 100000;
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        for (const auto& request_str : test_requests) {
            HttpRequest request;
            try {
                HttpRequestParser::parse(request_str, &request);
            } catch (...) {
                // 忽略错误
            }
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    std::cout << "原始解析器: " << iterations * 3 << " 个请求解析用时: " 
              << duration.count() << " 微秒" << std::endl;
    std::cout << "平均每个请求: " << duration.count() / (iterations * 3.0) << " 微秒" << std::endl;
    std::cout << "吞吐量: " << (iterations * 3 * 1000000.0) / duration.count() << " req/s" << std::endl;
}

void test_fast_parser_performance() {
    std::cout << "\n=== 测试快速解析器性能 ===" << std::endl;
    
    const int iterations = 100000;
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        for (const auto& request_str : test_requests) {
            FastHttpRequest fast_request;
            FastHttpParser::parse(request_str, fast_request);
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    std::cout << "快速解析器: " << iterations * 3 << " 个请求解析用时: " 
              << duration.count() << " 微秒" << std::endl;
    std::cout << "平均每个请求: " << duration.count() / (iterations * 3.0) << " 微秒" << std::endl;
    std::cout << "吞吐量: " << (iterations * 3 * 1000000.0) / duration.count() << " req/s" << std::endl;
}

void test_conversion_performance() {
    std::cout << "\n=== 测试转换性能 ===" << std::endl;
    
    const int iterations = 100000;
    FastHttpRequest fast_request;
    FastHttpParser::parse(test_requests[0], fast_request);
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        HttpRequest request;
        HttpRequestAdapter::convert(fast_request, request);
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    std::cout << "转换操作: " << iterations << " 次转换用时: " 
              << duration.count() << " 微秒" << std::endl;
    std::cout << "平均每次转换: " << duration.count() / (double)iterations << " 微秒" << std::endl;
}

void validate_parsing_correctness() {
    std::cout << "\n=== 验证解析正确性 ===" << std::endl;
    
    for (size_t i = 0; i < 3; ++i) {
        std::cout << "\n测试请求 " << (i + 1) << ":" << std::endl;
        
        // 使用原始解析器
        HttpRequest original_request;
        try {
            HttpRequestParser::parse(test_requests[i], &original_request);
        } catch (const std::exception& e) {
            std::cout << "原始解析器错误: " << e.what() << std::endl;
            continue;
        }
        
        // 使用快速解析器
        FastHttpRequest fast_request;
        if (!FastHttpParser::parse(test_requests[i], fast_request)) {
            std::cout << "快速解析器解析失败" << std::endl;
            continue;
        }
        
        // 转换并比较
        HttpRequest converted_request;
        HttpRequestAdapter::convert(fast_request, converted_request);
        
        // 比较结果
        bool method_match = original_request.getMethod() == converted_request.getMethod();
        bool url_match = original_request.getUrl() == converted_request.getUrl();
        bool version_match = original_request.getVersion() == converted_request.getVersion();
        bool body_match = original_request.getBody() == converted_request.getBody();
        
        std::cout << "方法匹配: " << (method_match ? "✓" : "✗") << std::endl;
        std::cout << "URL匹配: " << (url_match ? "✓" : "✗") << std::endl;
        std::cout << "版本匹配: " << (version_match ? "✓" : "✗") << std::endl;
        std::cout << "请求体匹配: " << (body_match ? "✓" : "✗") << std::endl;
        
        if (!method_match || !url_match || !version_match || !body_match) {
            std::cout << "⚠️  解析结果不一致!" << std::endl;
        } else {
            std::cout << "✓ 解析结果一致" << std::endl;
        }
    }
}

int main() {
    std::cout << "🚀 HTTP解析器性能测试" << std::endl;
    std::cout << "========================" << std::endl;
    
    validate_parsing_correctness();
    test_original_parser_performance();
    test_fast_parser_performance();
    test_conversion_performance();
    
    std::cout << "\n📊 性能测试完成！" << std::endl;
    
    return 0;
} 