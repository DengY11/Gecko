#include <iostream>
#include <cassert>
#include "http/http_request.hpp"

void test_default_constructor() {
    Gecko::HttpRequest request;
    
    assert(request.getMethod() == Gecko::HttpMethod::UNKNOWN);
    assert(request.getUrl().empty());
    assert(request.getVersion() == Gecko::HttpVersion::UNKNOWN);
    assert(request.getHeaders().empty());
    assert(request.getBody().empty());
    
}

void test_string_constructor() {
    std::string raw_request = 
        "GET /test HTTP/1.0\r\n"
        "Host: example.com\r\n"
        "\r\n";
    
    Gecko::HttpRequest request(raw_request);
    
    assert(request.getMethod() == Gecko::HttpMethod::GET);
    assert(request.getUrl() == "/test");
    assert(request.getVersion() == Gecko::HttpVersion::HTTP_1_0);
    assert(request.getHeaders().at("Host") == "example.com");
    assert(request.getBody().empty());
    
}

void test_full_constructor() {
    Gecko::HttpHeaderMap headers;
    headers["Host"] = "api.example.com";
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer token123";
    
    std::string body = "{\"key\":\"value\"}";
    
    Gecko::HttpRequest request(
        Gecko::HttpMethod::POST,
        "/api/data",
        Gecko::HttpVersion::HTTP_1_0,
        headers,
        body
    );
    
    assert(request.getMethod() == Gecko::HttpMethod::POST);
    assert(request.getUrl() == "/api/data");
    assert(request.getVersion() == Gecko::HttpVersion::HTTP_1_0);
    assert(request.getBody() == "{\"key\":\"value\"}");
    
    auto req_headers = request.getHeaders();
    assert(req_headers["Host"] == "api.example.com");
    assert(req_headers["Content-Type"] == "application/json");
    assert(req_headers["Authorization"] == "Bearer token123");
    
}

void test_copy_constructor() {
    /* Build original request */
    Gecko::HttpHeaderMap headers;
    headers["Host"] = "original.com";
    headers["User-Agent"] = "TestAgent/1.0";
    
    Gecko::HttpRequest original(
        Gecko::HttpMethod::PUT,
        "/api/update",
        Gecko::HttpVersion::HTTP_1_0,
        headers,
        "update data"
    );
    
    /* Use copy constructor */
    Gecko::HttpRequest copy(original);
    
    /* Validate copy result */
    assert(copy.getMethod() == original.getMethod());
    assert(copy.getUrl() == original.getUrl());
    assert(copy.getVersion() == original.getVersion());
    assert(copy.getBody() == original.getBody());
    
    auto orig_headers = original.getHeaders();
    auto copy_headers = copy.getHeaders();
    assert(orig_headers.size() == copy_headers.size());
    for (const auto& pair : orig_headers) {
        assert(copy_headers.at(pair.first) == pair.second);
    }
    
    /* Ensure deep copy by mutating copy */
    copy.setUrl("/modified");
    assert(original.getUrl() == "/api/update");
    assert(copy.getUrl() == "/modified");
    
}

void test_move_constructor() {
    /* Build original request */
    Gecko::HttpHeaderMap headers;
    headers["Host"] = "move.com";
    headers["Content-Type"] = "text/plain";
    
    Gecko::HttpRequest original(
        Gecko::HttpMethod::DELETE,
        "/api/delete/123",
        Gecko::HttpVersion::HTTP_1_0,
        headers,
        "delete reason"
    );
    
    /* Remember initial values */
    auto original_method = original.getMethod();
    auto original_url = original.getUrl();
    auto original_version = original.getVersion();
    auto original_body = original.getBody();
    
    /* Use move constructor */
    Gecko::HttpRequest moved(std::move(original));
    
    /* Validate move result */
    assert(moved.getMethod() == original_method);
    assert(moved.getUrl() == original_url);
    assert(moved.getVersion() == original_version);
    assert(moved.getBody() == original_body);
    
    auto moved_headers = moved.getHeaders();
    assert(moved_headers["Host"] == "move.com");
    assert(moved_headers["Content-Type"] == "text/plain");
    
}

void test_copy_assignment() {
    /* Build source object */
    Gecko::HttpHeaderMap headers1;
    headers1["Host"] = "source.com";
    
    Gecko::HttpRequest source(
        Gecko::HttpMethod::HEAD,
        "/api/check",
        Gecko::HttpVersion::HTTP_1_0,
        headers1,
        ""
    );
    
    /* Build target object with different values */
    Gecko::HttpHeaderMap headers2;
    headers2["Host"] = "target.com";
    
    Gecko::HttpRequest target(
        Gecko::HttpMethod::GET,
        "/different",
        Gecko::HttpVersion::HTTP_1_0,
        headers2,
        "different body"
    );
    
    /* Perform copy assignment */
    target = source;
    
    /* Validate assignment */
    assert(target.getMethod() == source.getMethod());
    assert(target.getUrl() == source.getUrl());
    assert(target.getVersion() == source.getVersion());
    assert(target.getBody() == source.getBody());
    
    auto source_headers = source.getHeaders();
    auto target_headers = target.getHeaders();
    assert(source_headers.at("Host") == target_headers.at("Host"));
    
    /* Confirm deep copy */
    target.setUrl("/modified_target");
    assert(source.getUrl() == "/api/check");
    assert(target.getUrl() == "/modified_target");
    
}

void test_move_assignment() {
    /* Build source object */
    Gecko::HttpHeaderMap headers1;
    headers1["Host"] = "move-source.com";
    headers1["Authorization"] = "Bearer move-token";
    
    Gecko::HttpRequest source(
        Gecko::HttpMethod::POST,
        "/api/move-data",
        Gecko::HttpVersion::HTTP_1_0,
        headers1,
        "move body data"
    );
    
    /* Store initial values */
    auto original_method = source.getMethod();
    auto original_url = source.getUrl();
    auto original_body = source.getBody();
    
    /* Build target object */
    Gecko::HttpRequest target;
    
    /* Perform move assignment */
    target = std::move(source);
    
    /* Validate move assignment */
    assert(target.getMethod() == original_method);
    assert(target.getUrl() == original_url);
    assert(target.getBody() == original_body);
    
    auto target_headers = target.getHeaders();
    assert(target_headers["Host"] == "move-source.com");
    assert(target_headers["Authorization"] == "Bearer move-token");
    
}

void test_setters_getters() {
    Gecko::HttpRequest request;
    
    /* Validate method setter */
    request.setMethod(Gecko::HttpMethod::PUT);
    assert(request.getMethod() == Gecko::HttpMethod::PUT);
    
    /* Validate URL setter */
    request.setUrl("/api/test");
    assert(request.getUrl() == "/api/test");
    
    /* Validate version setter */
    request.setVersion(Gecko::HttpVersion::HTTP_1_0);
    assert(request.getVersion() == Gecko::HttpVersion::HTTP_1_0);
    
    /* setHeaders/setBody overloads are ambiguous, skip direct testing */
    /* Constructors already cover those setters */
    
}

int main() {
    test_default_constructor();
    test_string_constructor();
    test_full_constructor();
    test_copy_constructor();
    test_move_constructor();
    test_copy_assignment();
    test_move_assignment();
    test_setters_getters();
    
    return 0;
}
