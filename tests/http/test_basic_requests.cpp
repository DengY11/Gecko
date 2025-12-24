#include <iostream>
#include <cassert>
#include "http/http_request.hpp"

void test_get_request() {
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
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] GET request test failed: " << e.what() << std::endl;
    }
}

void test_post_request() {
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
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] POST request test failed: " << e.what() << std::endl;
    }
}

void test_put_request() {
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
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] PUT request test failed: " << e.what() << std::endl;
    }
}

void test_delete_request() {
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
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] DELETE request test failed: " << e.what() << std::endl;
    }
}

void test_head_request() {
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
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] HEAD request test failed: " << e.what() << std::endl;
    }
}

int main() {
    test_get_request();
    test_post_request();
    test_put_request();
    test_delete_request();
    test_head_request();
    
    return 0;
}
