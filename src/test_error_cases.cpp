#include <iostream>
#include <cassert>
#include "http_request.hpp"

void test_invalid_request_line() {
    std::cout << "=== 测试无效请求行 ===" << std::endl;
    
    // 缺少HTTP版本
    std::string raw_request = "GET /api/users\r\n\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "✗ 应该抛出异常但没有" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ 正确捕获异常: " << e.what() << std::endl;
    }
}

void test_missing_headers_end() {
    std::cout << "=== 测试缺少headers结束标记 ===" << std::endl;
    
    std::string raw_request = 
        "GET /api/users HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "User-Agent: TestClient/1.0\r\n";  // 缺少 \r\n\r\n
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "✗ 应该抛出异常但没有" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ 正确捕获异常: " << e.what() << std::endl;
    }
}

void test_invalid_header_format() {
    std::cout << "=== 测试无效header格式 ===" << std::endl;
    
    std::string raw_request = 
        "GET /api/users HTTP/1.0\r\n"
        "Host api.example.com\r\n"  // 缺少冒号
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "✗ 应该抛出异常但没有" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ 正确捕获异常: " << e.what() << std::endl;
    }
}

void test_invalid_content_length() {
    std::cout << "=== 测试无效Content-Length ===" << std::endl;
    
    std::string raw_request = 
        "POST /api/data HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "Content-Length: invalid\r\n"  // 非数字
        "\r\n"
        "test data";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "✗ 应该抛出异常但没有" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ 正确捕获异常: " << e.what() << std::endl;
    }
}

void test_body_length_mismatch() {
    std::cout << "=== 测试body长度不匹配 ===" << std::endl;
    
    std::string raw_request = 
        "POST /api/data HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "Content-Length: 100\r\n"  // 声明100字节
        "\r\n"
        "short";  // 实际只有5字节
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "✗ 应该抛出异常但没有" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ 正确捕获异常: " << e.what() << std::endl;
    }
}

void test_null_pointer() {
    std::cout << "=== 测试空指针 ===" << std::endl;
    
    std::string raw_request = "GET / HTTP/1.0\r\n\r\n";
    
    try {
        Gecko::HttpRequestParser::parse(raw_request, nullptr);
        std::cerr << "✗ 应该抛出异常但没有" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ 正确捕获异常: " << e.what() << std::endl;
    }
}

void test_unknown_method() {
    std::cout << "=== 测试未知HTTP方法 ===" << std::endl;
    
    std::string raw_request = 
        "PATCH /api/users/1 HTTP/1.0\r\n"  // PATCH不在支持列表中
        "Host: api.example.com\r\n"
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        // 应该解析成功，但method为UNKNOWN
        assert(request.getMethod() == Gecko::HttpMethod::UNKNOWN);
        assert(request.getUrl() == "/api/users/1");
        
        std::cout << "✓ 未知方法正确处理为UNKNOWN" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ 不应该抛出异常: " << e.what() << std::endl;
    }
}

void test_empty_request() {
    std::cout << "=== 测试空请求 ===" << std::endl;
    
    std::string raw_request = "";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "✗ 应该抛出异常但没有" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✓ 正确捕获异常: " << e.what() << std::endl;
    }
}

void test_case_insensitive_headers() {
    std::cout << "=== 测试header大小写不敏感 ===" << std::endl;
    
    std::string raw_request = 
        "GET /api/users HTTP/1.0\r\n"
        "HOST: api.example.com\r\n"           // 大写
        "content-type: application/json\r\n"  // 小写
        "Content-Length: 0\r\n"              // 混合
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        auto headers = request.getHeaders();
        
        // 验证可以用不同大小写访问
        bool found_host = false;
        bool found_content_type = false;
        bool found_content_length = false;
        
        for (const auto& pair : headers) {
            if (pair.first == "HOST" || pair.first == "host" || pair.first == "Host") {
                found_host = true;
            }
            if (pair.first == "content-type" || pair.first == "Content-Type") {
                found_content_type = true;
            }
            if (pair.first == "Content-Length" || pair.first == "content-length") {
                found_content_length = true;
            }
        }
        
        assert(found_host && found_content_type && found_content_length);
        std::cout << "✓ header大小写不敏感测试通过" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ 测试失败: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "开始错误处理和边界情况测试..." << std::endl << std::endl;
    
    test_invalid_request_line();
    test_missing_headers_end();
    test_invalid_header_format();
    test_invalid_content_length();
    test_body_length_mismatch();
    test_null_pointer();
    test_unknown_method();
    test_empty_request();
    test_case_insensitive_headers();
    
    std::cout << std::endl << "错误处理和边界情况测试完成！" << std::endl;
    return 0;
} 