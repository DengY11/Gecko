#include "http_response.hpp"
#include <algorithm>
#include <string_view>
#include <stdexcept>
#include <cstring>

namespace Gecko {

HttpResponse HttpResponse::stockResponse(int statusCode) {
    HttpResponse response;
    response.setVersion(HttpVersion::HTTP_1_1);  /* Default to HTTP/1.1 */
    response.setStatusCode(statusCode);
    
    /* Use iterator to avoid redundant map lookups */
    auto it = statusCodeMap.find(statusCode);
    if (it != statusCodeMap.end()) {
        response.setReasonPhrase(it->second);  /* Copy is unavoidable here */
    } else {
        response.setReasonPhrase("Unknown");
    }
    return response;
}

/* TODO: optimize performance */
void HttpResponse::addHeader(const std::string &key, const std::string &value,
                             bool overwrite) {
    if (!overwrite && headers.find(key) != headers.end()) {
        throw std::invalid_argument(
            "Header already exists and overwrite is disabled");
    }
    headers[key] = value;
}

/* Estimate serialized size */
size_t HttpResponse::estimateSerializedSize() const {
    /* HTTP/1.1 200 OK\r\n ~= 15 bytes */
    size_t size = HttpVersionToString(version).length() + 1 + 
                  std::to_string(statusCode).length() + 1 + 
                  reasonPhrase.length() + 2;
    
    /* Headers */
    for (const auto& [key, value] : headers) {
        size += key.length() + 2 + value.length() + 2; /* key: value\r\n */
    }
    
    /* Content-Length header if not present */
    bool has_content_length = headers.find("Content-Length") != headers.end() ||
                             headers.find("content-length") != headers.end();
    if (!has_content_length) {
        size += 16 + std::to_string(body.length()).length() + 2; /* Content-Length: xxx\r\n */
    }
    
    size += 2; /* \r\n separator */
    size += body.length(); /* Body */
    
    /* Add 10% buffer */
    return size + (size / 10);
}

/* Serialize directly into a string with preallocation */
void HttpResponse::serializeTo(std::string& output) const {
    output.clear();
    output.reserve(estimateSerializedSize());
    
    /* Status line */
    output += HttpVersionToString(version);
    output += ' ';
    output += std::to_string(statusCode);
    output += ' ';
    output += reasonPhrase;
    output += "\r\n";
    
    /* Headers via const ref to avoid copies */
    const auto& headers_ref = headers;
    bool has_content_length = headers_ref.find("Content-Length") != headers_ref.end() ||
                             headers_ref.find("content-length") != headers_ref.end();
    
    /* Add Content-Length if missing */
    if (!has_content_length) {
        output += "Content-Length: ";
        output += std::to_string(body.length());
        output += "\r\n";
    }
    
    for (const auto& [key, value] : headers_ref) {
        output += key;
        output += ": ";
        output += value;
        output += "\r\n";
    }
    
    output += "\r\n";
    output += body;
}

/* HttpResponseSerializer implementation */
auto HttpResponseSerializer::serialize(const HttpResponse &response) -> std::string {
    std::string result;
    response.serializeTo(result);
    return result;
}

void HttpResponseSerializer::serializeTo(const HttpResponse &response, std::string& output) {
    response.serializeTo(output);
}

/* Serialize directly into a caller-provided buffer */
size_t HttpResponseSerializer::serializeToBuffer(const HttpResponse &response, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return 0;
    }
    
    char* current = buffer;
    char* end = buffer + buffer_size;
    
    /* Helper lambda for safe buffer writing */
    auto safe_write = [&current, end](std::string_view data) -> bool {
        if (current + data.length() >= end) {
            return false; /* Buffer overflow */
        }
        std::memcpy(current, data.data(), data.length());
        current += data.length();
        return true;
    };
    
    /* Status line */
    std::string version_str = HttpVersionToString(response.getVersion());
    std::string status_code_str = std::to_string(response.getStatusCode());
    
    if (!safe_write(version_str) || 
        !safe_write(" ") ||
        !safe_write(status_code_str) ||
        !safe_write(" ") ||
        !safe_write(response.getReasonPhrase()) ||
        !safe_write("\r\n")) {
        return 0;
    }
    
    /* Headers */
    const auto& headers = response.getHeaders();
    bool has_content_length = headers.find("Content-Length") != headers.end() ||
                             headers.find("content-length") != headers.end();
    
    /* Add Content-Length if missing */
    if (!has_content_length) {
        std::string content_length_str = std::to_string(response.getBody().length());
        if (!safe_write("Content-Length: ") ||
            !safe_write(content_length_str) ||
            !safe_write("\r\n")) {
            return 0;
        }
    }
    
    for (const auto& [key, value] : headers) {
        if (!safe_write(key) ||
            !safe_write(": ") ||
            !safe_write(value) ||
            !safe_write("\r\n")) {
            return 0;
        }
    }
    
    /* Empty line and body */
    if (!safe_write("\r\n") ||
        !safe_write(response.getBody())) {
        return 0;
    }
    
    return current - buffer;
}

} /* namespace Gecko */
