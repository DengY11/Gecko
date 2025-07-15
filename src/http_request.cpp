#include "http_request.hpp"
#include <sstream>
#include <stdexcept>

namespace Gecko {

std::string urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            std::string hex = str.substr(i + 1, 2);
            try {
                int value = std::stoi(hex, nullptr, 16);
                result += static_cast<char>(value);
                i += 2; 
            } catch (...) {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

auto stringToHttpMethod(std::string method) -> HttpMethod {
    static const std::map<std::string, HttpMethod> methods = {
        {"GET", HttpMethod::GET},
        {"POST", HttpMethod::POST},
        {"HEAD", HttpMethod::HEAD},
        {"PUT", HttpMethod::PUT},
        {"DELETE", HttpMethod::DELETE}};
    if (methods.find(method) != methods.end()) {
        return methods.at(method);
    }
    return HttpMethod::UNKNOWN;
}

auto HttpMethodToString(HttpMethod method) -> std::string {
    static const std::map<HttpMethod, std::string> methods = {
        {HttpMethod::GET, "GET"},
        {HttpMethod::POST, "POST"},
        {HttpMethod::HEAD, "HEAD"},
        {HttpMethod::PUT, "PUT"},
        {HttpMethod::DELETE, "DELETE"}};
    if (methods.find(method) != methods.end()) {
        return methods.at(method);
    }
    return "UNKNOWN";
}

auto stringToHttpVersion(std::string version) -> HttpVersion {
    static const std::map<std::string, HttpVersion> versions = {
        {"HTTP/1.0", HttpVersion::HTTP_1_0},
        {"HTTP/1.1", HttpVersion::HTTP_1_1}};
    if (versions.find(version) != versions.end()) {
        return versions.at(version);
    }
    return HttpVersion::UNKNOWN;
}

auto HttpVersionToString(HttpVersion version) -> std::string {
    static const std::map<HttpVersion, std::string> versions = {
        {HttpVersion::HTTP_1_0, "HTTP/1.0"},
        {HttpVersion::HTTP_1_1, "HTTP/1.1"}};
    if (versions.find(version) != versions.end()) {
        return versions.at(version);
    }
    return "UNKNOWN";
}

HttpRequest::HttpRequest()
: method(HttpMethod::UNKNOWN), version(HttpVersion::UNKNOWN) {}

HttpRequest::HttpRequest(std::string request) {
    HttpRequestParser::parse(request, this);
}

HttpRequest::HttpRequest(const HttpRequest &other) {
    method = other.method;
    url = other.url;
    version = other.version;
    headers = other.headers;
    body = other.body;
    query_params = other.query_params;
}

HttpRequest::HttpRequest(HttpRequest &&other) {
    method = std::move(other.method);
    url = std::move(other.url);
    version = std::move(other.version);
    headers = std::move(other.headers);
    body = std::move(other.body);
    query_params = std::move(other.query_params);
}

HttpRequest::HttpRequest(HttpMethod method, HttpUrl url, HttpVersion version, 
                        HttpHeaderMap headers, HttpBody body)
    : method(method), url(url), version(version), headers(headers), body(body) {
    parseQueryParams();
}

HttpRequest &HttpRequest::operator=(const HttpRequest &other) {
    method = other.method;
    url = other.url;
    version = other.version;
    headers = other.headers;
    body = other.body;
    query_params = other.query_params;
    return *this;
}

HttpRequest &HttpRequest::operator=(HttpRequest &&other) {
    method = std::move(other.method);
    url = std::move(other.url);
    version = std::move(other.version);
    headers = std::move(other.headers);
    body = std::move(other.body);
    query_params = std::move(other.query_params);
    return *this;
}

void HttpRequest::parseQueryParams() {
    query_params.clear();
    size_t query_start = url.find('?');
    if (query_start == std::string::npos) {
        return; 
    }
    std::string query_string = url.substr(query_start + 1);
    //split by &
    std::stringstream ss(query_string);
    std::string param;
    while (std::getline(ss, param, '&')) {
        if (param.empty()) continue;
        size_t eq_pos = param.find('=');
        if (eq_pos == std::string::npos) {
            query_params[param] = "";
        } else {
            std::string key = param.substr(0, eq_pos);
            std::string value = param.substr(eq_pos + 1);
            // for url like /search?q=hello%20world&name=%E5%BC%A0%E4%B8%89&test=a+b
            // will return return a pair of "q" and "hello world"
            query_params[key] = urlDecode(value);
        }
    }
}

void trim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char ch) { return !std::isspace(ch); })
            .base(),
            s.end());
}

void HttpRequestParser::parse(std::string originRequestString,
                              HttpRequest *httpRequest) {
    if (!httpRequest) {
        throw std::invalid_argument("HttpRequest is null");
        return;
    }
    const std::string crlf = "\r\n";
    const std::string double_crlf = "\r\n\r\n";

    size_t request_line_end = originRequestString.find(crlf);
    if (request_line_end == std::string::npos) {
        throw std::invalid_argument("request line not found");
        return;
    }
    std::string request_line = originRequestString.substr(0, request_line_end);
    std::stringstream request_line_stream(request_line);
    std::string method_str, url_str, version_str;
    request_line_stream >> method_str >> url_str >> version_str;

    if (method_str.empty() || url_str.empty() || version_str.empty()) {
        throw std::invalid_argument("request line is invalid");
        return;
    }
    httpRequest->method = stringToHttpMethod(method_str);
    httpRequest->url = url_str;
    httpRequest->parseQueryParams(); 
    httpRequest->version = stringToHttpVersion(version_str);

    size_t headers_start = request_line_end + crlf.length();
    size_t headers_end = originRequestString.find(double_crlf, headers_start);
    if (headers_end == std::string::npos) {
        throw std::invalid_argument("headers not found");
        return;
    }
    std::string headers_block =
        originRequestString.substr(headers_start, headers_end - headers_start);
    std::stringstream headers_stream(headers_block);
    std::string header_line;
    HttpHeaderMap headers;
    while (std::getline(headers_stream, header_line) && !header_line.empty() &&
        header_line != "\r\n") {
        if (header_line.back() == '\r') {
            header_line.pop_back();
        }
        size_t colon_pos = header_line.find(":");
        if (colon_pos == std::string::npos) {
            throw std::invalid_argument("header line is invalid");
            return;
        }
        std::string key = header_line.substr(0, colon_pos);
        std::string value = header_line.substr(colon_pos + 1);
        trim(key);
        trim(value);
        headers[key] = value;
    }

    size_t body_start = headers_end + double_crlf.length();
    auto it = headers.find("Content-Length");
    if (it != headers.end()) {
        try {
            int content_length = std::stoi(it->second);
            if (content_length > 0) {
                if (originRequestString.length() >= body_start + content_length) {
                    httpRequest->body =
                        originRequestString.substr(body_start, content_length);
                } else {
                    throw std::runtime_error("Actual body length is less than expected");
                }
            }
        } catch (const std::exception &e) {
            throw std::runtime_error("Content-Length is invalid");
        }
    }
    httpRequest->headers = std::move(headers);
}

} // namespace Gecko
