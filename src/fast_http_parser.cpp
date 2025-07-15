#include "fast_http_parser.hpp"
#include "http_request.hpp"
#include <cstring>
#include <charconv>

namespace Gecko {

bool FastHttpParser::parse(const char* data, size_t size, FastHttpRequest& request) {
    return parse(std::string_view(data, size), request);
}

bool FastHttpParser::parse(std::string_view data, FastHttpRequest& request) {
    request.reset();
    request.raw_data = data.data();
    request.raw_size = data.size();
    
    const char* request_line_end = find_crlf(data.data(), data.data() + data.size());
    if (!request_line_end) {
        return false; 
    }
    
    std::string_view request_line(data.data(), request_line_end - data.data());
    if (!parse_request_line(request_line, request)) {
        return false;
    }
    
    const char* headers_start = request_line_end + 2; 
    const char* headers_end = find_double_crlf(headers_start, data.data() + data.size());
    if (!headers_end) {
        return false; 
    }
    
    std::string_view headers_block(headers_start, headers_end - headers_start);
    if (!parse_headers(headers_block, request)) {
        return false;
    }
    
    parse_query_params(request.url, request);
    
    const char* body_start = headers_end + 4; 
    size_t content_length = parse_content_length(request.headers);
    
    if (content_length > 0) {
        size_t available_body_size = data.data() + data.size() - body_start;
        if (available_body_size >= content_length) {
            request.body = std::string_view(body_start, content_length);
        } else {
            return false; 
        }
    }
    
    return true;
}

bool FastHttpParser::parse_request_line(std::string_view line, FastHttpRequest& request) {
    size_t first_space = line.find(' ');
    if (first_space == std::string_view::npos) return false;
    
    size_t second_space = line.find(' ', first_space + 1);
    if (second_space == std::string_view::npos) return false;
    
    std::string_view method_str = line.substr(0, first_space);
    request.url = line.substr(first_space + 1, second_space - first_space - 1);
    request.version = line.substr(second_space + 1);
    
    request.method = parse_method(method_str);
    
    return request.method != FastHttpMethod::UNKNOWN && 
           !request.url.empty() && !request.version.empty();
}

bool FastHttpParser::parse_headers(std::string_view headers_block, FastHttpRequest& request) {
    const char* current = headers_block.data();
    const char* end = headers_block.data() + headers_block.size();
    
    while (current < end) {
        const char* line_end = find_crlf(current, end);
        if (!line_end) {
            line_end = end;
        }
        
        std::string_view line(current, line_end - current);
        if (line.empty()) {
            break; 
        }
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string_view::npos) {
            return false; 
        }
        std::string_view key = trim(line.substr(0, colon_pos));
        std::string_view value = trim(line.substr(colon_pos + 1));
        
        request.headers[key] = value;
        current = line_end;
        if (current < end && current[0] == '\r') current++;
        if (current < end && current[0] == '\n') current++;
    }
    
    return true;
}

void FastHttpParser::parse_query_params(std::string_view url, FastHttpRequest& request) {
    size_t query_start = url.find('?');
    if (query_start == std::string_view::npos) {
        return; 
    }
    
    std::string_view query_string = url.substr(query_start + 1);
    size_t start = 0;
    while (start < query_string.size()) {
        size_t end = query_string.find('&', start);
        if (end == std::string_view::npos) {
            end = query_string.size();
        }
        std::string_view param = query_string.substr(start, end - start);
        if (!param.empty()) {
            size_t eq_pos = param.find('=');
            if (eq_pos != std::string_view::npos) {
                std::string_view key = param.substr(0, eq_pos);
                std::string_view value = param.substr(eq_pos + 1);
                request.query_params[key] = value;
            } else {
                request.query_params[param] = {};
            }
        }
        
        start = end + 1;
    }
}

size_t FastHttpParser::parse_content_length(const FastHeaderMap& headers) {
    for (const auto& [key, value] : headers) {
        if (key.size() == 14 && 
            std::equal(key.begin(), key.end(), "content-length",
                      [](char a, char b) { return std::tolower(a) == std::tolower(b); })) {
            size_t result = 0;
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
            if (ec == std::errc{}) {
                return result;
            }
            break;
        }
    }
    return 0;
}

void HttpRequestAdapter::convert(const FastHttpRequest& fast_req, HttpRequest& request) {
    switch (fast_req.method) {
        case FastHttpMethod::GET:    request.setMethod(HttpMethod::GET); break;
        case FastHttpMethod::POST:   request.setMethod(HttpMethod::POST); break;
        case FastHttpMethod::HEAD:   request.setMethod(HttpMethod::HEAD); break;
        case FastHttpMethod::PUT:    request.setMethod(HttpMethod::PUT); break;
        case FastHttpMethod::DELETE: request.setMethod(HttpMethod::DELETE); break;
        default: request.setMethod(HttpMethod::UNKNOWN); break;
    }
    
    request.setUrl(std::string(fast_req.url));
     if (fast_req.version == "HTTP/1.0") {
         request.setVersion(HttpVersion::HTTP_1_0);
     } else if (fast_req.version == "HTTP/1.1") {
         request.setVersion(HttpVersion::HTTP_1_1);
     } else {
         request.setVersion(HttpVersion::UNKNOWN);
     }
    
    HttpHeaderMap headers;
    for (const auto& [key, value] : fast_req.headers) {
        headers[std::string(key)] = std::string(value);
    }
    request.setHeaders(std::move(headers));
    request.setBody(std::string(fast_req.body));
}

} // namespace Gecko 