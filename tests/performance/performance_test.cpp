#include "fast_http_parser.hpp"
#include "http_request.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <string>

using namespace Gecko;
using namespace std::chrono;

/* Sample HTTP requests */
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
    std::cout << "=== Benchmark: baseline parser ===" << std::endl;
    
    const int iterations = 100000;
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        for (const auto& request_str : test_requests) {
            HttpRequest request;
            try {
                HttpRequestParser::parse(request_str, &request);
            } catch (...) {
                /* Ignore parse failures */
            }
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    std::cout << "Baseline parser: " << iterations * 3 << " requests parsed in " 
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Avg per request: " << duration.count() / (iterations * 3.0) << " microseconds" << std::endl;
    std::cout << "Throughput: " << (iterations * 3 * 1000000.0) / duration.count() << " req/s" << std::endl;
}

void test_fast_parser_performance() {
    std::cout << "\n=== Benchmark: fast parser ===" << std::endl;
    
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
    
    std::cout << "Fast parser: " << iterations * 3 << " requests parsed in " 
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Avg per request: " << duration.count() / (iterations * 3.0) << " microseconds" << std::endl;
    std::cout << "Throughput: " << (iterations * 3 * 1000000.0) / duration.count() << " req/s" << std::endl;
}

void test_conversion_performance() {
    std::cout << "\n=== Benchmark: conversion ===" << std::endl;
    
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
    
    std::cout << "Conversion: " << iterations << " iterations took " 
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Average per conversion: " << duration.count() / (double)iterations << " microseconds" << std::endl;
}

void validate_parsing_correctness() {
    std::cout << "\n=== Validation: parser correctness ===" << std::endl;
    
    for (size_t i = 0; i < 3; ++i) {
        std::cout << "\nTest request " << (i + 1) << ":" << std::endl;
        
        /* Use baseline parser */
        HttpRequest original_request;
        try {
            HttpRequestParser::parse(test_requests[i], &original_request);
        } catch (const std::exception& e) {
            std::cout << "Baseline parser error: " << e.what() << std::endl;
            continue;
        }
        
        /* Use fast parser */
        FastHttpRequest fast_request;
        if (!FastHttpParser::parse(test_requests[i], fast_request)) {
            std::cout << "Fast parser failed" << std::endl;
            continue;
        }
        
        /* Convert and compare */
        HttpRequest converted_request;
        HttpRequestAdapter::convert(fast_request, converted_request);
        
        /* Compare results */
        bool method_match = original_request.getMethod() == converted_request.getMethod();
        bool url_match = original_request.getUrl() == converted_request.getUrl();
        bool version_match = original_request.getVersion() == converted_request.getVersion();
        bool body_match = original_request.getBody() == converted_request.getBody();
        
        std::cout << "Method match: " << (method_match ? "[OK]" : "[FAIL]") << std::endl;
        std::cout << "URL match: " << (url_match ? "[OK]" : "[FAIL]") << std::endl;
        std::cout << "Version match: " << (version_match ? "[OK]" : "[FAIL]") << std::endl;
        std::cout << "Body match: " << (body_match ? "[OK]" : "[FAIL]") << std::endl;
        
        if (!method_match || !url_match || !version_match || !body_match) {
            std::cout << "[WARN] Parsed results differ!" << std::endl;
        } else {
            std::cout << "[OK] Parsed results match" << std::endl;
        }
    }
}

int main() {
    std::cout << "[START] HTTP parser performance tests" << std::endl;
    std::cout << "========================" << std::endl;
    
    validate_parsing_correctness();
    test_original_parser_performance();
    test_fast_parser_performance();
    test_conversion_performance();
    
    std::cout << "\n[STATS] Performance tests finished" << std::endl;
    
    return 0;
}
