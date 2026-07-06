#include "pnad/backend/HttpServer.hpp"

#include "pnad/constants/BackendConstants.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace asset_discovery::backend {
namespace {

namespace sha1 {
    inline std::uint32_t leftRotate(std::uint32_t value, int bits) {
        return (value << bits) | (value >> (32 - bits));
    }

    inline std::string calculate(const std::string& input) {
        std::uint32_t h0 = 0x67452301;
        std::uint32_t h1 = 0xEFCDAB89;
        std::uint32_t h2 = 0x98BADCFE;
        std::uint32_t h3 = 0x10325476;
        std::uint32_t h4 = 0xC3D2E1F0;

        std::vector<std::uint8_t> message(input.begin(), input.end());
        std::uint64_t origLenBits = message.size() * 8;

        message.push_back(0x80);
        while ((message.size() + 8) % 64 != 0) {
            message.push_back(0x00);
        }

        for (int i = 7; i >= 0; --i) {
            message.push_back((origLenBits >> (i * 8)) & 0xFF);
        }

        for (std::size_t chunkOffset = 0; chunkOffset < message.size(); chunkOffset += 64) {
            std::uint32_t w[80] = {0};
            for (int i = 0; i < 16; ++i) {
                w[i] = (message[chunkOffset + i * 4] << 24) |
                       (message[chunkOffset + i * 4 + 1] << 16) |
                       (message[chunkOffset + i * 4 + 2] << 8) |
                       (message[chunkOffset + i * 4 + 3]);
            }

            for (int i = 16; i < 80; ++i) {
                w[i] = leftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
            }

            std::uint32_t a = h0;
            std::uint32_t b = h1;
            std::uint32_t c = h2;
            std::uint32_t d = h3;
            std::uint32_t e = h4;

            for (int i = 0; i < 80; ++i) {
                std::uint32_t f, k;
                if (i < 20) {
                    f = (b & c) | ((~b) & d);
                    k = 0x5A827999;
                } else if (i < 40) {
                    f = b ^ c ^ d;
                    k = 0x6ED9EBA1;
                } else if (i < 60) {
                    f = (b & c) | (b & d) | (c & d);
                    k = 0x8F1BBCDC;
                } else {
                    f = b ^ c ^ d;
                    k = 0xCA62C1D6;
                }

                std::uint32_t temp = leftRotate(a, 5) + f + e + k + w[i];
                e = d;
                d = c;
                c = leftRotate(b, 30);
                b = a;
                a = temp;
            }

            h0 += a;
            h1 += b;
            h2 += c;
            h3 += d;
            h4 += e;
        }

        std::uint8_t hash[20];
        for (int i = 0; i < 4; ++i) {
            hash[i] = (h0 >> (24 - i * 8)) & 0xFF;
            hash[4 + i] = (h1 >> (24 - i * 8)) & 0xFF;
            hash[8 + i] = (h2 >> (24 - i * 8)) & 0xFF;
            hash[12 + i] = (h3 >> (24 - i * 8)) & 0xFF;
            hash[16 + i] = (h4 >> (24 - i * 8)) & 0xFF;
        }

        return std::string(reinterpret_cast<char*>(hash), 20);
    }
}

namespace base64 {
    inline std::string encode(const std::string& input) {
        const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string output;
        output.reserve(((input.size() + 2) / 3) * 4);
        std::uint32_t val = 0;
        int valb = -6;
        for (std::uint8_t c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                output.push_back(alphabet[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) {
            output.push_back(alphabet[((val << 8) >> (valb + 8)) & 0x3F]);
        }
        while (output.size() % 4 != 0) {
            output.push_back('=');
        }
        return output;
    }
}

std::string trimCarriageReturn(std::string value)
{
    if (!value.empty() && value.back() == '\r') {
        value.pop_back();
    }
    return value;
}

HttpRequest parseRequest(const std::string& raw)
{
    HttpRequest request;
    std::istringstream input(raw);
    std::string line;
    if (std::getline(input, line)) {
        line = trimCarriageReturn(line);
        std::istringstream requestLine(line);
        requestLine >> request.method >> request.target;
        const auto queryPosition = request.target.find('?');
        if (queryPosition == std::string::npos) {
            request.path = request.target;
        } else {
            request.path = request.target.substr(0, queryPosition);
            request.query = request.target.substr(queryPosition + 1);
        }
    }

    while (std::getline(input, line)) {
        line = trimCarriageReturn(line);
        if (line.empty()) {
            break;
        }
        const auto separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }
        auto value = line.substr(separator + 1);
        while (!value.empty() && value.front() == ' ') {
            value.erase(value.begin());
        }
        request.headers.emplace(line.substr(0, separator), std::move(value));
    }
    return request;
}

std::string serializeResponse(const HttpResponse& response)
{
    std::ostringstream output;
    output << "HTTP/1.1 " << response.status << ' ' << httpStatusText(response.status) << "\r\n"
           << "Content-Type: " << response.contentType << "\r\n"
           << "Content-Length: " << response.body.size() << "\r\n"
           << "Connection: close\r\n"
           << "\r\n"
           << response.body;
    return output.str();
}

} // namespace

HttpServer::HttpServer(Handler handler, WsHandler wsHandler)
    : handler_(std::move(handler)),
      wsHandler_(std::move(wsHandler))
{
}

HttpServer::~HttpServer()
{
    stop();
}

std::optional<std::string> HttpServer::serve(const std::string& listenAddress, std::uint16_t port)
{
    serverFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        return std::string("socket failed: ") + std::strerror(errno);
    }

    int reuse = 1;
    (void)::setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, listenAddress.c_str(), &address.sin_addr) != 1) {
        stop();
        return "listen address must be an IPv4 address";
    }

    if (::bind(serverFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        const std::string message = std::string("bind failed: ") + std::strerror(errno);
        stop();
        return message;
    }

    if (::listen(serverFd_, constants::backend::SocketBacklog) < 0) {
        const std::string message = std::string("listen failed: ") + std::strerror(errno);
        stop();
        return message;
    }

    while (serverFd_ >= 0) {
        const int client = ::accept(serverFd_, nullptr, nullptr);
        if (client < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (serverFd_ < 0) {
                break;
            }
            return std::string("accept failed: ") + std::strerror(errno);
        }

        char buffer[constants::backend::HttpRequestBufferSize] = {};
        const auto readCount = ::read(client, buffer, sizeof(buffer) - 1);
        HttpResponse response;
        HttpRequest request;
        if (readCount <= 0) {
            response.status = 400;
            response.body = R"({"success":false,"error":{"code":"bad_request","message":"empty request"}})";
        } else {
            request = parseRequest(std::string(buffer, static_cast<std::size_t>(readCount)));
            response = handler_(request);
        }

        if (response.status == 101 && wsHandler_) {
            std::string key;
            for (const auto& pair : request.headers) {
                std::string k = pair.first;
                std::transform(k.begin(), k.end(), k.begin(), ::tolower);
                if (k == "sec-websocket-key") {
                    key = pair.second;
                    break;
                }
            }
            std::string accept = base64::encode(sha1::calculate(key + constants::backend::WebSocketGuid));
            std::ostringstream responseStream;
            responseStream << "HTTP/1.1 101 Switching Protocols\r\n"
                           << "Upgrade: websocket\r\n"
                           << "Connection: Upgrade\r\n"
                           << "Sec-WebSocket-Accept: " << accept << "\r\n\r\n";
            std::string handshake = responseStream.str();
            (void)::write(client, handshake.data(), handshake.size());
            wsHandler_(client);
        } else {
            const auto serialized = serializeResponse(response);
            (void)::write(client, serialized.data(), serialized.size());
            ::close(client);
        }
    }

    return std::nullopt;
}

void HttpServer::stop()
{
    if (serverFd_ >= 0) {
        ::shutdown(serverFd_, SHUT_RDWR);
        ::close(serverFd_);
        serverFd_ = -1;
    }
}

std::string httpStatusText(int status)
{
    switch (status) {
    case 200:
        return "OK";
    case 202:
        return "Accepted";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 409:
        return "Conflict";
    case 500:
        return "Internal Server Error";
    default:
        return "OK";
    }
}

} // namespace asset_discovery::backend
