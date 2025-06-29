#include <iostream>
#include <cassert>
#include "http_request.hpp"

void test_whitespace_trimming() {
    std::cout << "=== 测试空格清理 ===" << std::endl;
    
    std::string raw_request = 
        "GET /api/test HTTP/1.0\r\n"
        "Host:   example.com   \r\n"        // Host值有前后空格
        "  User-Agent  :TestClient/1.0  \r\n"  // key和value都有空格
        "Content-Type: application/json\r\n"
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        auto headers = request.getHeaders();
        assert(headers["Host"] == "example.com");  // 空格应该被清理
        assert(headers["User-Agent"] == "TestClient/1.0");  // 空格应该被清理
        assert(headers["Content-Type"] == "application/json");
        
        std::cout << "✓ 空格清理测试通过" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ 空格清理测试失败: " << e.what() << std::endl;
    }
}

void test_long_url() {
    std::cout << "=== 测试长URL ===" << std::endl;
    
    // 创建一个很长的URL
    std::string long_path = "/api/v1/users/search?query=";
    for (int i = 0; i < 100; ++i) {
        long_path += "test" + std::to_string(i) + "&param" + std::to_string(i) + "=value" + std::to_string(i) + "&";
    }
    long_path += "end=true";
    
    std::string raw_request = 
        "GET " + long_path + " HTTP/1.0\r\n"
        "Host: api.longurl.com\r\n"
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        assert(request.getMethod() == Gecko::HttpMethod::GET);
        assert(request.getUrl() == long_path);
        assert(request.getVersion() == Gecko::HttpVersion::HTTP_1_0);
        
        std::cout << "✓ 长URL测试通过 (URL长度: " << long_path.length() << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ 长URL测试失败: " << e.what() << std::endl;
    }
}

void test_unicode_in_headers() {
    std::cout << "=== 测试header中的特殊字符 ===" << std::endl;
    
    std::string raw_request = 
        "POST /api/upload HTTP/1.0\r\n"
        "Host: upload.example.com\r\n"
        "X-Custom-Header: value-with-dashes_and_underscores.and.dots\r\n"
        "Authorization: Bearer abc123!@#$%^&*()_+-=[]{};':\",./<>?\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "special data!";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        auto headers = request.getHeaders();
        assert(headers["X-Custom-Header"] == "value-with-dashes_and_underscores.and.dots");
        assert(headers["Authorization"] == "Bearer abc123!@#$%^&*()_+-=[]{};':\",./<>?");
        assert(request.getBody() == "special data!");
        
        std::cout << "✓ 特殊字符测试通过" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ 特殊字符测试失败: " << e.what() << std::endl;
    }
}

void test_multiple_header_values() {
    std::cout << "=== 测试多个相同header名称 ===" << std::endl;
    
    std::string raw_request = 
        "GET /api/data HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "Accept: application/json\r\n"
        "Accept: text/html\r\n"           // 重复的Accept header
        "X-Custom: value1\r\n"
        "X-Custom: value2\r\n"           // 重复的自定义header
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        auto headers = request.getHeaders();
        // 注意：由于使用的是map，后面的值会覆盖前面的值
        assert(headers["Accept"] == "text/html");  // 最后一个值
        assert(headers["X-Custom"] == "value2");   // 最后一个值
        
        std::cout << "✓ 多个相同header测试通过（后值覆盖前值）" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ 多个相同header测试失败: " << e.what() << std::endl;
    }
}

void test_empty_header_value() {
    std::cout << "=== 测试空header值 ===" << std::endl;
    
    std::string raw_request = 
        "GET /api/test HTTP/1.0\r\n"
        "Host: example.com\r\n"
        "X-Empty-Header:\r\n"            // 空值
        "X-Space-Header: \r\n"          // 只有空格的值
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        auto headers = request.getHeaders();
        assert(headers["X-Empty-Header"] == "");  // 空字符串
        assert(headers["X-Space-Header"] == "");  // 空格被trim掉
        
        std::cout << "✓ 空header值测试通过" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ 空header值测试失败: " << e.what() << std::endl;
    }
}

void test_large_body() {
    std::cout << "=== 测试大body ===" << std::endl;
    
    // 创建一个大的body
    std::string large_body;
    for (int i = 0; i < 1000; ++i) {
        large_body += "This is line " + std::to_string(i) + " of the large body content. ";
    }
    
    std::string raw_request = 
        "POST /api/upload HTTP/1.0\r\n"
        "Host: upload.example.com\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(large_body.length()) + "\r\n"
        "\r\n" + large_body;
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        assert(request.getMethod() == Gecko::HttpMethod::POST);
        assert(request.getBody() == large_body);
        assert(request.getBody().length() == large_body.length());
        
        std::cout << "✓ 大body测试通过 (body长度: " << large_body.length() << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ 大body测试失败: " << e.what() << std::endl;
    }
}

void test_zero_content_length() {
    std::cout << "=== 测试Content-Length为0 ===" << std::endl;
    
    std::string raw_request = 
        "POST /api/ping HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        assert(request.getMethod() == Gecko::HttpMethod::POST);
        assert(request.getBody().empty());
        
        auto headers = request.getHeaders();
        assert(headers["Content-Length"] == "0");
        
        std::cout << "✓ Content-Length为0测试通过" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Content-Length为0测试失败: " << e.what() << std::endl;
    }
}

void test_utility_functions() {
    std::cout << "=== 测试工具函数 ===" << std::endl;
    
    // 测试HTTP方法转换
    assert(Gecko::stringToHttpMethod("GET") == Gecko::HttpMethod::GET);
    assert(Gecko::stringToHttpMethod("POST") == Gecko::HttpMethod::POST);
    assert(Gecko::stringToHttpMethod("INVALID") == Gecko::HttpMethod::UNKNOWN);
    
    assert(Gecko::HttpMethodToString(Gecko::HttpMethod::GET) == "GET");
    assert(Gecko::HttpMethodToString(Gecko::HttpMethod::POST) == "POST");
    assert(Gecko::HttpMethodToString(Gecko::HttpMethod::UNKNOWN) == "UNKNOWN");
    
    // 测试HTTP版本转换
    assert(Gecko::stringToHttpVersion("HTTP/1.0") == Gecko::HttpVersion::HTTP_1_0);
    assert(Gecko::stringToHttpVersion("HTTP/2.0") == Gecko::HttpVersion::UNKNOWN);
    
    assert(Gecko::HttpVersionToString(Gecko::HttpVersion::HTTP_1_0) == "HTTP/1.0");
    assert(Gecko::HttpVersionToString(Gecko::HttpVersion::UNKNOWN) == "UNKNOWN");
    
    std::cout << "✓ 工具函数测试通过" << std::endl;
}

int main() {
    std::cout << "开始特殊情况测试..." << std::endl << std::endl;
    
    test_whitespace_trimming();
    test_long_url();
    test_unicode_in_headers();
    test_multiple_header_values();
    test_empty_header_value();
    test_large_body();
    test_zero_content_length();
    test_utility_functions();
    
    std::cout << std::endl << "特殊情况测试完成！" << std::endl;
    return 0;
} 