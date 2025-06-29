#include <iostream>
#include <cassert>
#include "http_request.hpp"

void test_get_request() {
    std::cout << "=== 测试GET请求 ===" << std::endl;
    
    std::string raw_request = 
        "GET /api/users?page=1&limit=10 HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "User-Agent: TestClient/1.0\r\n"
        "Accept: application/json\r\n"
        "\r\n";

    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        assert(request.getMethod() == Gecko::HttpMethod::GET);
        assert(request.getUrl() == "/api/users?page=1&limit=10");
        assert(request.getVersion() == Gecko::HttpVersion::HTTP_1_0);
        assert(request.getBody().empty());
        
        auto headers = request.getHeaders();
        assert(headers["Host"] == "api.example.com");
        assert(headers["User-Agent"] == "TestClient/1.0");
        assert(headers["Accept"] == "application/json");
        
        std::cout << "✓ GET请求测试通过" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ GET请求测试失败: " << e.what() << std::endl;
    }
}

void test_post_request() {
    std::cout << "=== 测试POST请求 ===" << std::endl;
    
    std::string raw_request = 
        "POST /api/login HTTP/1.0\r\n"
        "Host: auth.example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 45\r\n"
        "\r\n"
        "{\"username\":\"admin\",\"password\":\"secret123\"}";

    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        assert(request.getMethod() == Gecko::HttpMethod::POST);
        assert(request.getUrl() == "/api/login");
        assert(request.getVersion() == Gecko::HttpVersion::HTTP_1_0);
        assert(request.getBody() == "{\"username\":\"admin\",\"password\":\"secret123\"}");
        
        auto headers = request.getHeaders();
        assert(headers["Content-Type"] == "application/json");
        assert(headers["Content-Length"] == "45");
        
        std::cout << "✓ POST请求测试通过" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ POST请求测试失败: " << e.what() << std::endl;
    }
}

void test_put_request() {
    std::cout << "=== 测试PUT请求 ===" << std::endl;
    
    std::string raw_request = 
        "PUT /api/users/123 HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 25\r\n"
        "\r\n"
        "name=John&email=john@doe";

    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        assert(request.getMethod() == Gecko::HttpMethod::PUT);
        assert(request.getUrl() == "/api/users/123");
        assert(request.getBody() == "name=John&email=john@doe");
        
        std::cout << "✓ PUT请求测试通过" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ PUT请求测试失败: " << e.what() << std::endl;
    }
}

void test_delete_request() {
    std::cout << "=== 测试DELETE请求 ===" << std::endl;
    
    std::string raw_request = 
        "DELETE /api/users/456 HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "Authorization: Bearer token123\r\n"
        "\r\n";

    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        assert(request.getMethod() == Gecko::HttpMethod::DELETE);
        assert(request.getUrl() == "/api/users/456");
        assert(request.getBody().empty());
        
        auto headers = request.getHeaders();
        assert(headers["Authorization"] == "Bearer token123");
        
        std::cout << "✓ DELETE请求测试通过" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ DELETE请求测试失败: " << e.what() << std::endl;
    }
}

void test_head_request() {
    std::cout << "=== 测试HEAD请求 ===" << std::endl;
    
    std::string raw_request = 
        "HEAD /status HTTP/1.0\r\n"
        "Host: health.example.com\r\n"
        "User-Agent: HealthChecker/2.0\r\n"
        "\r\n";

    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        assert(request.getMethod() == Gecko::HttpMethod::HEAD);
        assert(request.getUrl() == "/status");
        assert(request.getBody().empty());
        
        std::cout << "✓ HEAD请求测试通过" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ HEAD请求测试失败: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "开始基本HTTP请求测试..." << std::endl << std::endl;
    
    test_get_request();
    test_post_request();
    test_put_request();
    test_delete_request();
    test_head_request();
    
    std::cout << std::endl << "基本HTTP请求测试完成！" << std::endl;
    return 0;
} 