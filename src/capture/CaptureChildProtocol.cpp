#include "pnad/capture/CaptureChildProtocol.hpp"

#include <charconv>
#include <sstream>

namespace asset_discovery::capture {
namespace {

std::optional<std::uint64_t> parseUnsigned(const std::string& value)
{
    std::uint64_t parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end) {
        return std::nullopt;
    }
    return parsed;
}

std::string escapeField(const std::string& value)
{
    std::string output;
    output.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\':
            output += "\\\\";
            break;
        case '\n':
            output += "\\n";
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

std::string unescapeField(const std::string& value)
{
    std::string output;
    output.reserve(value.size());
    bool escaped = false;
    for (const char character : value) {
        if (escaped) {
            switch (character) {
            case 'n':
                output += '\n';
                break;
            case 't':
                output += '\t';
                break;
            default:
                output += character;
                break;
            }
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
            continue;
        }
        output += character;
    }
    if (escaped) {
        output += '\\';
    }
    return output;
}

} // namespace

std::string captureChildMessageTypeName(CaptureChildMessageType type)
{
    switch (type) {
    case CaptureChildMessageType::Started:
        return "started";
    case CaptureChildMessageType::NewData:
        return "new_data";
    case CaptureChildMessageType::Dropped:
        return "dropped";
    case CaptureChildMessageType::Error:
        return "error";
    case CaptureChildMessageType::Stopped:
        return "stopped";
    }
    return "error";
}

std::optional<CaptureChildMessageType> parseCaptureChildMessageType(const std::string& value)
{
    if (value == "started") {
        return CaptureChildMessageType::Started;
    }
    if (value == "new_data") {
        return CaptureChildMessageType::NewData;
    }
    if (value == "dropped") {
        return CaptureChildMessageType::Dropped;
    }
    if (value == "error") {
        return CaptureChildMessageType::Error;
    }
    if (value == "stopped") {
        return CaptureChildMessageType::Stopped;
    }
    return std::nullopt;
}

std::string serializeCaptureChildMessage(const CaptureChildMessage& message)
{
    std::ostringstream output;
    output << "event=" << captureChildMessageTypeName(message.type)
           << "\tpackets=" << message.packetCount
           << "\tdropped=" << message.droppedCount;

    if (!message.capturePath.empty()) {
        output << "\tcapture_path=" << escapeField(message.capturePath);
    }
    if (!message.message.empty()) {
        output << "\tmessage=" << escapeField(message.message);
    }
    for (const auto& field : message.fields) {
        output << '\t' << field.first << '=' << escapeField(field.second);
    }
    output << '\n';
    return output.str();
}

std::optional<CaptureChildMessage> parseCaptureChildMessage(const std::string& line)
{
    CaptureChildMessage message;
    bool foundType = false;
    std::istringstream input(line);
    std::string token;
    while (std::getline(input, token, '\t')) {
        if (!token.empty() && token.back() == '\n') {
            token.pop_back();
        }
        if (!token.empty() && token.back() == '\r') {
            token.pop_back();
        }
        const auto separator = token.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const auto key = token.substr(0, separator);
        const auto value = unescapeField(token.substr(separator + 1));

        if (key == "event") {
            const auto type = parseCaptureChildMessageType(value);
            if (!type.has_value()) {
                return std::nullopt;
            }
            message.type = *type;
            foundType = true;
        } else if (key == "packets") {
            const auto parsed = parseUnsigned(value);
            if (!parsed.has_value()) {
                return std::nullopt;
            }
            message.packetCount = *parsed;
        } else if (key == "dropped") {
            const auto parsed = parseUnsigned(value);
            if (!parsed.has_value()) {
                return std::nullopt;
            }
            message.droppedCount = *parsed;
        } else if (key == "capture_path") {
            message.capturePath = value;
        } else if (key == "message") {
            message.message = value;
        } else {
            message.fields[key] = value;
        }
    }

    if (!foundType) {
        return std::nullopt;
    }
    return message;
}

} // namespace asset_discovery::capture
