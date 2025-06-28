#ifndef HTTP_REQUEST
#define HTTP_REQUEST
#include <algorithm>
#include <map>
#include <string>

namespace Gecko {

enum class HttpMethod { GET, POST, HEAD, PUT, DELETE, UNKNOWN };

inline auto stringToHttpMethod(std::string method) -> HttpMethod;
inline auto HttpMethodToString(HttpMethod method) -> std::string;

enum class HttpVersion { HTTP_1_0, UNKNOWN };

inline auto stringToHttpVersion(std::string version) -> HttpVersion;
inline auto HttpVersionToString(HttpVersion version) -> std::string;

struct CaseInsensitiveCompare {
    bool operator()(const std::string &a, const std::string &b) const {
        return std::lexicographical_compare(
            a.begin(), a.end(), b.begin(), b.end(),
            [](char ca, char cb) { return std::tolower(ca) < std::tolower(cb); });
    }
};

using HttpHeaderMap =
std::map<std::string, std::string, CaseInsensitiveCompare>;
using HttpUrl = std::string;
using HttpBody = std::string;

class HttpRequest {
public:
    friend class HttpRequestParser;
    HttpRequest();
    HttpRequest(std::string);
    HttpRequest(const HttpRequest &other);
    HttpRequest(HttpRequest &&other);
    HttpRequest(HttpMethod, HttpUrl, HttpVersion, HttpHeaderMap, HttpBody);

    HttpRequest &operator=(const HttpRequest &other);
    HttpRequest &operator=(HttpRequest &&other);

    HttpMethod getMethod() const { return method; }
    HttpUrl getUrl() const { return url; }
    HttpVersion getVersion() const { return version; }
    HttpHeaderMap getHeaders() const { return headers; }
    HttpBody getBody() const { return body; }

    void setMethod(HttpMethod method) { this->method = method; }
    void setUrl(HttpUrl url) { this->url = url; }
    void setVersion(HttpVersion version) { this->version = version; }
    void setHeaders(HttpHeaderMap headers) { this->headers = headers; }
    void setHeaders(HttpHeaderMap &&headers) {
        this->headers = std::move(headers);
    }
    void setBody(HttpBody body) { this->body = body; }
    void setBody(HttpBody &&body) { this->body = std::move(body); }

private:
    HttpMethod method;
    HttpUrl url;
    HttpVersion version;
    HttpHeaderMap headers;
    HttpBody body;
};

void trim(std::string &s);

struct HttpRequestParser {
    static void parse(std::string originRequestString, HttpRequest *request);
};

} // namespace Gecko

#endif
