#include "pnad/backend/RestApi.hpp"

#include <sstream>
#include <utility>

namespace asset_discovery::backend {

std::string jsonEscape(const std::string& value)
{
    std::string output;
    output.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output += character;
            break;
        }
    }
    return output;
}

std::string jsonString(const std::string& value)
{
    return "\"" + jsonEscape(value) + "\"";
}

HttpResponse jsonSuccess(std::string dataJson, int status)
{
    HttpResponse response;
    response.status = status;
    response.body = "{\"success\":true,\"data\":" + std::move(dataJson) + "}";
    return response;
}

HttpResponse jsonError(int status, const std::string& code, const std::string& message)
{
    HttpResponse response;
    response.status = status;
    response.body = "{\"success\":false,\"error\":{\"code\":"
        + jsonString(code)
        + ",\"message\":"
        + jsonString(message)
        + "}}";
    return response;
}

} // namespace asset_discovery::backend
