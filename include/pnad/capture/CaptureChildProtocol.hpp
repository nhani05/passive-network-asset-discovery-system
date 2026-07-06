#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace asset_discovery::capture {

enum class CaptureChildMessageType {
    Started,
    NewData,
    Dropped,
    Error,
    Stopped,
};

struct CaptureChildMessage {
    CaptureChildMessageType type = CaptureChildMessageType::Started;
    std::uint64_t packetCount = 0;
    std::uint64_t droppedCount = 0;
    std::string capturePath;
    std::string message;
    std::map<std::string, std::string> fields;
};

std::string captureChildMessageTypeName(CaptureChildMessageType type);
std::optional<CaptureChildMessageType> parseCaptureChildMessageType(const std::string& value);
std::string serializeCaptureChildMessage(const CaptureChildMessage& message);
std::optional<CaptureChildMessage> parseCaptureChildMessage(const std::string& line);

} // namespace asset_discovery::capture
