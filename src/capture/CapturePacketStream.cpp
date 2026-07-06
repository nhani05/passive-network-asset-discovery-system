#include "pnad/capture/CapturePacketStream.hpp"

#include <charconv>
#include <limits>
#include <sstream>

namespace asset_discovery::capture {
namespace {

constexpr const char* framePrefix = "PNAD_PACKET_V1";

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

std::optional<std::int64_t> parseSigned(const std::string& value)
{
    std::int64_t parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<LinkType> parseLinkType(const std::string& value)
{
    if (value == "ethernet") {
        return LinkType::Ethernet;
    }
    return std::nullopt;
}

std::string linkTypeStreamName(LinkType linkType)
{
    switch (linkType) {
    case LinkType::Ethernet:
        return "ethernet";
    }
    return "ethernet";
}

} // namespace

bool writePacketStreamFrame(std::ostream& output, const OfflinePacket& packet)
{
    output << framePrefix
           << ' ' << packet.timestamp.seconds
           << ' ' << packet.timestamp.microseconds
           << ' ' << linkTypeStreamName(packet.linkType)
           << ' ' << packet.capturedLength
           << ' ' << packet.originalLength
           << ' ' << packet.bytes.size()
           << '\n';
    if (!packet.bytes.empty()) {
        output.write(
            reinterpret_cast<const char*>(packet.bytes.data()),
            static_cast<std::streamsize>(packet.bytes.size()));
    }
    output << '\n';
    return static_cast<bool>(output);
}

PacketStreamReadResult readPacketStreamFrames(std::istream& input, std::size_t maxFrames)
{
    PacketStreamReadResult result;
    std::string header;
    while (result.packets.size() < maxFrames && std::getline(input, header)) {
        if (header.empty()) {
            continue;
        }

        std::istringstream headerInput(header);
        std::string prefix;
        std::string secondsText;
        std::string microsText;
        std::string linkText;
        std::string capturedText;
        std::string originalText;
        std::string sizeText;
        headerInput >> prefix >> secondsText >> microsText >> linkText >> capturedText >> originalText >> sizeText;
        if (prefix != framePrefix) {
            result.error = "invalid packet stream frame prefix";
            return result;
        }

        const auto seconds = parseSigned(secondsText);
        const auto micros = parseSigned(microsText);
        const auto linkType = parseLinkType(linkText);
        const auto capturedLength = parseUnsigned(capturedText);
        const auto originalLength = parseUnsigned(originalText);
        const auto byteCount = parseUnsigned(sizeText);
        if (!seconds.has_value()
            || !micros.has_value()
            || !linkType.has_value()
            || !capturedLength.has_value()
            || !originalLength.has_value()
            || !byteCount.has_value()
            || *capturedLength > std::numeric_limits<std::uint32_t>::max()
            || *originalLength > std::numeric_limits<std::uint32_t>::max()
            || *byteCount > std::numeric_limits<std::uint32_t>::max()) {
            result.error = "invalid packet stream frame header";
            return result;
        }

        OfflinePacket packet;
        packet.timestamp.seconds = *seconds;
        packet.timestamp.microseconds = *micros;
        packet.linkType = *linkType;
        packet.capturedLength = static_cast<std::uint32_t>(*capturedLength);
        packet.originalLength = static_cast<std::uint32_t>(*originalLength);
        packet.bytes.resize(static_cast<std::size_t>(*byteCount));
        if (*byteCount > 0) {
            input.read(
                reinterpret_cast<char*>(packet.bytes.data()),
                static_cast<std::streamsize>(*byteCount));
            if (input.gcount() != static_cast<std::streamsize>(*byteCount)) {
                result.error = "packet stream ended mid-frame";
                return result;
            }
        }

        char newline = '\0';
        input.get(newline);
        if (!input || newline != '\n') {
            result.error = "packet stream frame missing trailing newline";
            return result;
        }

        result.packets.push_back(std::move(packet));
    }

    result.endOfStream = input.eof();
    return result;
}

std::string packetStreamFormatName()
{
    return framePrefix;
}

} // namespace asset_discovery::capture
