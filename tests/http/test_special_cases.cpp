#include <iostream>
#include <cassert>
#include "http_request.hpp"

void test_whitespace_trimming() {
    std::string raw_request = 
        "GET /api/test HTTP/1.0\r\n"
        "Host:   example.com   \r\n"        /* Host value padded */
        "  User-Agent  :TestClient/1.0  \r\n"  /* Key and value padded */
        "Content-Type: application/json\r\n"
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        auto headers = request.getHeaders();
        assert(headers["Host"] == "example.com");  /* Whitespace should be trimmed */
        assert(headers["User-Agent"] == "TestClient/1.0");  /* Whitespace should be trimmed */
        assert(headers["Content-Type"] == "application/json");
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Whitespace trimming test failed: " << e.what() << std::endl;
    }
}

void test_long_url() {
    /* Build long URL */
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
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Long URL test failed: " << e.what() << std::endl;
    }
}

void test_unicode_in_headers() {
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
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Special character test failed: " << e.what() << std::endl;
    }
}

void test_multiple_header_values() {
    std::string raw_request = 
        "GET /api/data HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "Accept: application/json\r\n"
        "Accept: text/html\r\n"           /* Duplicate Accept header */
        "X-Custom: value1\r\n"
        "X-Custom: value2\r\n"           /* Duplicate custom header */
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        auto headers = request.getHeaders();
        /* Later values overwrite earlier ones because map stores unique keys */
        assert(headers["Accept"] == "text/html");  /* Uses last value */
        assert(headers["X-Custom"] == "value2");   /* Uses last value */
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Duplicate header test failed: " << e.what() << std::endl;
    }
}

void test_empty_header_value() {
    std::string raw_request = 
        "GET /api/test HTTP/1.0\r\n"
        "Host: example.com\r\n"
        "X-Empty-Header:\r\n"            /* Empty header value */
        "X-Space-Header: \r\n"          /* Value contains whitespace only */
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        auto headers = request.getHeaders();
        assert(headers["X-Empty-Header"] == "");  /* Empty string */
        assert(headers["X-Space-Header"] == "");  /* Whitespace trimmed */
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Empty header test failed: " << e.what() << std::endl;
    }
}

void test_large_body() {
    /* Build large body */
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
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Large body test failed: " << e.what() << std::endl;
    }
}

void test_zero_content_length() {
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
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Zero Content-Length test failed: " << e.what() << std::endl;
    }
}

void test_utility_functions() {
    /* Validate HTTP method helpers */
    assert(Gecko::stringToHttpMethod("GET") == Gecko::HttpMethod::GET);
    assert(Gecko::stringToHttpMethod("POST") == Gecko::HttpMethod::POST);
    assert(Gecko::stringToHttpMethod("PATCH") == Gecko::HttpMethod::PATCH);
    assert(Gecko::stringToHttpMethod("OPTIONS") == Gecko::HttpMethod::OPTIONS);
    assert(Gecko::stringToHttpMethod("INVALID") == Gecko::HttpMethod::UNKNOWN);
    
    assert(Gecko::HttpMethodToString(Gecko::HttpMethod::GET) == "GET");
    assert(Gecko::HttpMethodToString(Gecko::HttpMethod::POST) == "POST");
    assert(Gecko::HttpMethodToString(Gecko::HttpMethod::PATCH) == "PATCH");
    assert(Gecko::HttpMethodToString(Gecko::HttpMethod::OPTIONS) == "OPTIONS");
    assert(Gecko::HttpMethodToString(Gecko::HttpMethod::UNKNOWN) == "UNKNOWN");
    
    /* Validate HTTP version helpers */
    assert(Gecko::stringToHttpVersion("HTTP/1.0") == Gecko::HttpVersion::HTTP_1_0);
    assert(Gecko::stringToHttpVersion("HTTP/2.0") == Gecko::HttpVersion::UNKNOWN);
    
    assert(Gecko::HttpVersionToString(Gecko::HttpVersion::HTTP_1_0) == "HTTP/1.0");
    assert(Gecko::HttpVersionToString(Gecko::HttpVersion::UNKNOWN) == "UNKNOWN");
    
}

int main() {
    test_whitespace_trimming();
    test_long_url();
    test_unicode_in_headers();
    test_multiple_header_values();
    test_empty_header_value();
    test_large_body();
    test_zero_content_length();
    test_utility_functions();
    
    return 0;
}
