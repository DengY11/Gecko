#include "http_response.hpp"
#include <stdexcept>

namespace Gecko {

HttpResponse HttpResponse::stockResponse(int statusCode) {
    HttpResponse response;
    response.setVersion(HttpVersion::HTTP_1_0);
    response.setStatusCode(statusCode);
    std::string reasonPhrase;
    if (statusCodeMap.find(statusCode) != statusCodeMap.end()) {
        reasonPhrase = statusCodeMap.at(statusCode);
    } else {
        reasonPhrase = "Unknown";
    }
    response.setReasonPhrase(reasonPhrase);
    return response;
}

void HttpResponse::addHeader(const std::string &key, const std::string &value,
                             bool overwrite) {
    if (!overwrite && headers.find(key) != headers.end()) {
        throw std::invalid_argument(
            "Header already exists and overwrite is disabled");
    }
    headers[key] = value;
}

auto HttpResponseSerializer::serialize(const HttpResponse &response)
-> std::string {
    std::string serializedResponse;
    serializedResponse += HttpVersionToString(response.getVersion());
    serializedResponse += " " + std::to_string(response.getStatusCode());
    serializedResponse += " " + response.getReasonPhrase() + "\r\n";
    
    auto headers = response.getHeaders();
    
    if (headers.find("Content-Length") == headers.end() && headers.find("content-length") == headers.end()) {
        headers["Content-Length"] = std::to_string(response.getBody().length());
    }
    
    for (const auto &header : headers) {
        serializedResponse += header.first + ": " + header.second + "\r\n";
    }
    serializedResponse += "\r\n";
    serializedResponse += response.getBody();
    return serializedResponse;
}

} // namespace Gecko
