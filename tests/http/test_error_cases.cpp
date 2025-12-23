#include <iostream>
#include <cassert>
#include "http_request.hpp"

void test_invalid_request_line() {
    /* Missing HTTP version */
    std::string raw_request = "GET /api/users\r\n\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "[FAIL] Expected exception but none was thrown" << std::endl;
    } catch (const std::exception&) {
    }
}

void test_missing_headers_end() {
    std::string raw_request = 
        "GET /api/users HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "User-Agent: TestClient/1.0\r\n";  /* Missing \\r\\n\\r\\n */
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "[FAIL] Expected exception but none was thrown" << std::endl;
    } catch (const std::exception&) {
    }
}

void test_invalid_header_format() {
    std::string raw_request = 
        "GET /api/users HTTP/1.0\r\n"
        "Host api.example.com\r\n"  /* Missing colon */
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "[FAIL] Expected exception but none was thrown" << std::endl;
    } catch (const std::exception&) {
    }
}

void test_invalid_content_length() {
    std::string raw_request = 
        "POST /api/data HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "Content-Length: invalid\r\n"  /* Non-numeric length */
        "\r\n"
        "test data";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "[FAIL] Expected exception but none was thrown" << std::endl;
    } catch (const std::exception&) {
    }
}

void test_body_length_mismatch() {
    std::string raw_request = 
        "POST /api/data HTTP/1.0\r\n"
        "Host: api.example.com\r\n"
        "Content-Length: 100\r\n"  /* Declares 100 bytes */
        "\r\n"
        "short";  /* Only five bytes */
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "[FAIL] Expected exception but none was thrown" << std::endl;
    } catch (const std::exception&) {
    }
}

void test_null_pointer() {
    std::string raw_request = "GET / HTTP/1.0\r\n\r\n";
    
    try {
        Gecko::HttpRequestParser::parse(raw_request, nullptr);
        std::cerr << "[FAIL] Expected exception but none was thrown" << std::endl;
    } catch (const std::exception&) {
    }
}

void test_unknown_method() {
    std::string raw_request = 
        "PATCH /api/users/1 HTTP/1.0\r\n"  /* PATCH not supported */
        "Host: api.example.com\r\n"
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        /* Should parse but method becomes UNKNOWN */
        assert(request.getMethod() == Gecko::HttpMethod::UNKNOWN);
        assert(request.getUrl() == "/api/users/1");
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Unexpected exception: " << e.what() << std::endl;
    }
}

void test_empty_request() {
    std::string raw_request = "";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        std::cerr << "[FAIL] Expected exception but none was thrown" << std::endl;
    } catch (const std::exception&) {
    }
}

void test_case_insensitive_headers() {
    std::string raw_request = 
        "GET /api/users HTTP/1.0\r\n"
        "HOST: api.example.com\r\n"           /* Uppercase host key */
        "content-type: application/json\r\n"  /* Lowercase content type */
        "Content-Length: 0\r\n"              /* Mixed casing */
        "\r\n";
    
    Gecko::HttpRequest request;
    try {
        Gecko::HttpRequestParser::parse(raw_request, &request);
        
        auto headers = request.getHeaders();
        
        /* Validate case-insensitive lookup */
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
        
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Header case-insensitive test failed: " << e.what() << std::endl;
    }
}

int main() {
    test_invalid_request_line();
    test_missing_headers_end();
    test_invalid_header_format();
    test_invalid_content_length();
    test_body_length_mismatch();
    test_null_pointer();
    test_unknown_method();
    test_empty_request();
    test_case_insensitive_headers();
    
    return 0;
}
