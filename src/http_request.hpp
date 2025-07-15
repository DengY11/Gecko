#ifndef HTTP_REQUEST
#define HTTP_REQUEST
#include <algorithm>
#include <map>
#include <string>

namespace Gecko {

enum class HttpMethod { GET, POST, HEAD, PUT, DELETE, UNKNOWN };

auto stringToHttpMethod(std::string method) -> HttpMethod;
auto HttpMethodToString(HttpMethod method) -> std::string;

enum class HttpVersion { HTTP_1_0, HTTP_1_1, UNKNOWN };

auto stringToHttpVersion(std::string version) -> HttpVersion;
auto HttpVersionToString(HttpVersion version) -> std::string;

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
using HttpQueryMap = std::map<std::string, std::string>;

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
    const HttpVersion& getVersion() const { return version; }
    const HttpHeaderMap& getHeaders() const { return headers; }
    const HttpBody& getBody() const { return body; }
    const HttpQueryMap& getQueryParams() const { return query_params; }

    void setMethod(HttpMethod method) { this->method = method; }
    void setUrl(HttpUrl url) { 
        this->url = url; 
        parseQueryParams(); 
    }
    void setVersion(HttpVersion version) { this->version = version; }
    //void setHeaders(HttpHeaderMap headers) { this->headers = headers; }
    void setHeaders(const HttpHeaderMap &headers) { this->headers = headers; }
    void setHeaders(HttpHeaderMap &&headers) {
        this->headers = std::move(headers);
    }
    //void setBody(HttpBody body) { this->body = body; }
    void setBody(const HttpBody &body) { this->body = body; }
    void setBody(HttpBody &&body) { this->body = std::move(body); }
    std::string getQueryParam(const std::string& key) const {
        auto it = query_params.find(key);
        return it != query_params.end() ? it->second : "";
    }

private:
    HttpMethod method;
    HttpUrl url;
    HttpVersion version;
    HttpHeaderMap headers;
    HttpBody body;
    HttpQueryMap query_params; 
    void parseQueryParams();
};

void trim(std::string &s);
std::string urlDecode(const std::string& str);



struct HttpRequestParser {
    static void parse(std::string originRequestString, HttpRequest *request);
};

} // namespace Gecko

#endif
