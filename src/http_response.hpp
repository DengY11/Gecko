#ifndef HTTP_RESPONSE
#define HTTP_RESPONSE
#include "http_request.hpp"
#include <string>

namespace Gecko {
struct HttpResponseSerializer;

const std::map<int, std::string> statusCodeMap{
    {200, "OK"},
    {201, "Created"},
    {204, "No Content"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {304, "Not Modified"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {409, "Conflict"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
};

class HttpResponse {
public:
    HttpResponse() = default;
    HttpResponse(const HttpResponse &other) = default;
    HttpResponse(HttpResponse &&other) = default;
    HttpResponse& operator=(const HttpResponse &other) = default;
    HttpResponse& operator=(HttpResponse &&other) = default;

    static auto stockResponse(int statusCode) -> HttpResponse;
    void setVersion(HttpVersion version) { this->version = version; }
    void setStatusCode(int statusCode) { this->statusCode = statusCode; }
    void setReasonPhrase(const std::string &reasonPhrase) {
        this->reasonPhrase = reasonPhrase;
    }
    void setReasonPhrase(std::string &&reasonPhrase) {
        this->reasonPhrase = std::move(reasonPhrase);
    }
    void setBody(const HttpBody &body) { this->body = body; }
    void setBody(HttpBody &&body) { this->body = std::move(body); }
    void addHeader(const std::string &key, const std::string &value,
                   bool overwrite = true);

    HttpVersion getVersion() const { return version; }
    int getStatusCode() const { return statusCode; }
    std::string getReasonPhrase() const { return reasonPhrase; }
    HttpHeaderMap getHeaders() const { return headers; }
    HttpBody getBody() const { return body; }

private:
    friend class HttpResponseSerializer;
    HttpVersion version;
    int statusCode = 200;
    std::string reasonPhrase = "OK";
    HttpHeaderMap headers;
    HttpBody body;
};

struct HttpResponseSerializer {
    static auto serialize(const HttpResponse &response) -> std::string;
};

} // namespace Gecko

#endif
