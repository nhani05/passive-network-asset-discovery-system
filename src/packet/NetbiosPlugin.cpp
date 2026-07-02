#include "pnad/packet/NetbiosPlugin.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace asset_discovery::parser {
namespace {

constexpr std::uint16_t netbiosNameServicePort = 137;
constexpr std::size_t nbnsHeaderLength = 12;

bool isNetbiosNameServicePort(const UdpDatagram& udp)
{
    return udp.sourcePort == netbiosNameServicePort || udp.destinationPort == netbiosNameServicePort;
}

std::uint16_t readBigEndianUInt16(ByteView bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
                                      | static_cast<std::uint16_t>(bytes[offset + 1]));
}

std::string trimNetbiosName(std::string value)
{
    while (!value.empty() && (value.back() == ' ' || value.back() == '\0')) {
        value.pop_back();
    }
    return value;
}

std::optional<std::string> decodeEncodedNetbiosName(ByteView payload, std::size_t offset)
{
    if (offset + 32 > payload.size) {
        return std::nullopt;
    }

    std::string name;
    for (std::size_t index = 0; index < 16; ++index) {
        const auto high = payload[offset + (index * 2)];
        const auto low = payload[offset + (index * 2) + 1];
        if (high < 'A' || high > 'P' || low < 'A' || low > 'P') {
            return std::nullopt;
        }
        const auto decoded = static_cast<char>(((high - 'A') << 4U) | (low - 'A'));
        if (index < 15) {
            name.push_back(decoded);
        }
    }
    return trimNetbiosName(name);
}

std::optional<std::string> readNetbiosName(ByteView payload, std::size_t& offset, std::size_t depth = 0)
{
    if (offset >= payload.size || depth > 8) {
        return std::nullopt;
    }

    const auto length = payload[offset];
    if (length == 0) {
        ++offset;
        return ".";
    }
    if ((length & 0xc0U) == 0xc0U) {
        if (offset + 1 >= payload.size) {
            return std::nullopt;
        }
        const auto pointer = static_cast<std::size_t>(((length & 0x3fU) << 8U) | payload[offset + 1]);
        if (pointer >= payload.size) {
            return std::nullopt;
        }
        offset += 2;
        auto pointedOffset = pointer;
        return readNetbiosName(payload, pointedOffset, depth + 1);
    }
    if (length != 32 || offset + 1 + length > payload.size) {
        return std::nullopt;
    }

    const auto decodedName = decodeEncodedNetbiosName(payload, offset + 1);
    offset += 1 + length;
    while (offset < payload.size && payload[offset] != 0) {
        const auto scopeLength = payload[offset++];
        if (offset + scopeLength > payload.size) {
            return std::nullopt;
        }
        offset += scopeLength;
    }
    if (offset < payload.size && payload[offset] == 0) {
        ++offset;
    }
    return decodedName;
}

std::optional<AssetObservation> parseNetbiosPayload(const PacketContext& context)
{
    if (!context.ethernet.has_value() || !context.ipv4.has_value() || !context.udp.has_value()) {
        return std::nullopt;
    }
    const auto payload = context.udp->payload;
    if (payload.size < nbnsHeaderLength) {
        return std::nullopt;
    }

    AssetObservation observation;
    observation.macAddress = context.ethernet->sourceMac;
    observation.ipAddress = context.ipv4->sourceIp;
    observation.sourceId = sourceIdNetbios;
    observation.eventType = ObservationEventType::Seen;
    observation.confidence = 0.9F;
    observation.timestamp = context.timestamp;
    addObservedMetadata(observation, "udp.source_port", std::to_string(context.udp->sourcePort));
    addObservedMetadata(observation, "udp.destination_port", std::to_string(context.udp->destinationPort));

    const auto questionCount = readBigEndianUInt16(payload, 4);
    const auto answerCount = readBigEndianUInt16(payload, 6);
    const auto authorityCount = readBigEndianUInt16(payload, 8);
    const auto additionalCount = readBigEndianUInt16(payload, 10);

    std::size_t offset = nbnsHeaderLength;
    for (std::uint16_t index = 0; index < questionCount; ++index) {
        auto name = readNetbiosName(payload, offset);
        if (!name.has_value() || offset + 4 > payload.size) {
            return std::nullopt;
        }
        if (!name->empty() && *name != ".") {
            addObservedMetadata(observation, "netbios.names", *name);
        }
        offset += 4;
    }

    const auto recordCount = static_cast<std::uint32_t>(answerCount)
        + static_cast<std::uint32_t>(authorityCount)
        + static_cast<std::uint32_t>(additionalCount);
    for (std::uint32_t index = 0; index < recordCount; ++index) {
        auto name = readNetbiosName(payload, offset);
        if (!name.has_value() || offset + 10 > payload.size) {
            return std::nullopt;
        }
        if (!name->empty() && *name != ".") {
            addObservedMetadata(observation, "netbios.names", *name);
        }
        const auto dataLength = readBigEndianUInt16(payload, offset + 8);
        offset += 10;
        if (offset + dataLength > payload.size) {
            return std::nullopt;
        }
        offset += dataLength;
    }

    return observation;
}

} // namespace

std::string NetbiosPlugin::id() const
{
    return sourceIdNetbios;
}

ParserMatch NetbiosPlugin::match(const PacketContext& context) const
{
    if (context.udp.has_value() && isNetbiosNameServicePort(*context.udp) && context.udp->payload.size >= nbnsHeaderLength) {
        return {70};
    }
    return {};
}

std::vector<AssetObservation> NetbiosPlugin::parse(const PacketContext& context) const
{
    if (!match(context).matched()) {
        return {};
    }

    const auto observation = parseNetbiosPayload(context);
    if (!observation.has_value()) {
        return {};
    }
    return {*observation};
}

} // namespace asset_discovery::parser
