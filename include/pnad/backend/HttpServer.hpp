#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace asset_discovery::backend {

struct HttpRequest {
    std::string method;
    std::string target;
    std::string path;
    std::string query;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json";
    std::string body = "{}";
};

class HttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;
    using WsHandler = std::function<void(int)>;

    explicit HttpServer(Handler handler, WsHandler wsHandler = nullptr);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    std::optional<std::string> serve(const std::string& listenAddress, std::uint16_t port);
    void stop();

private:
    Handler handler_;
    WsHandler wsHandler_ = nullptr;
    int serverFd_ = -1;
};

std::string httpStatusText(int status);

} // namespace asset_discovery::backend
