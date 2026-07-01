#include "plugins/parser/DhcpPlugin.hpp"

#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace asset_discovery::parser {
namespace {

constexpr std::uint16_t dhcpServerPort = 67;
constexpr std::uint16_t dhcpClientPort = 68;
constexpr std::size_t dhcpFixedHeaderLength = 236;
constexpr std::uint32_t dhcpMagicCookie = 0x63825363;

bool isDhcpPortPair(const UdpDatagram& udp)
{
    return (udp.sourcePort == dhcpServerPort || udp.sourcePort == dhcpClientPort)
        && (udp.destinationPort == dhcpServerPort || udp.destinationPort == dhcpClientPort);
}

std::uint32_t readBigEndianUInt32(ByteView bytes, std::size_t offset)
{
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U)
        | static_cast<std::uint32_t>(bytes[offset + 3]);
}

std::string formatMacAddress(ByteView bytes, std::size_t offset)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < 6; ++i) {
        if (i > 0) {
            output << ':';
        }
        output << std::setw(2) << static_cast<int>(bytes[offset + i]);
    }
    return output.str();
}

std::string formatIpv4Address(ByteView bytes, std::size_t offset)
{
    std::ostringstream output;
    for (std::size_t i = 0; i < 4; ++i) {
        if (i > 0) {
            output << '.';
        }
        output << static_cast<int>(bytes[offset + i]);
    }
    return output.str();
}

std::optional<AssetObservation> parseDhcpPayload(
    ByteView payload,
    ObservationTimestamp timestamp,
    const UdpDatagram& udp)
{
    if (payload.size < dhcpFixedHeaderLength + 4) {
        return std::nullopt;
    }
    if (readBigEndianUInt32(payload, dhcpFixedHeaderLength) != dhcpMagicCookie) {
        return std::nullopt;
    }

    AssetObservation observation;
    observation.macAddress = formatMacAddress(payload, 28);
    observation.sourceId = sourceIdDhcp;
    observation.eventType = ObservationEventType::Update;
    observation.confidence = 1.0F;
    observation.timestamp = timestamp;
    observation.metadata["udp.source_port"] = std::to_string(udp.sourcePort);
    observation.metadata["udp.destination_port"] = std::to_string(udp.destinationPort);

    const auto yiaddr = formatIpv4Address(payload, 16);
    const auto ciaddr = formatIpv4Address(payload, 12);
    if (yiaddr != "0.0.0.0") {
        observation.ipAddress = yiaddr;
    } else if (ciaddr != "0.0.0.0") {
        observation.ipAddress = ciaddr;
    }

    std::size_t offset = dhcpFixedHeaderLength + 4;
    while (offset < payload.size) {
        const std::uint8_t option = payload[offset++];
        if (option == 255) {
            break;
        }
        if (option == 0) {
            continue;
        }
        if (offset >= payload.size) {
            return std::nullopt;
        }
        const std::size_t length = payload[offset++];
        if (payload.size - offset < length) {
            return std::nullopt;
        }
        if (option == 12) {
            observation.hostname = std::string(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                payload.begin() + static_cast<std::ptrdiff_t>(offset + length));
            observation.metadata["dhcp.option.hostname"] = *observation.hostname;
        } else if (option == 50 && length == 4 && !observation.ipAddress.has_value()) {
            observation.ipAddress = formatIpv4Address(payload, offset);
        } else if (option == 53 && length == 1) {
            observation.metadata["dhcp.message_type"] = std::to_string(payload[offset]);
        }
        offset += length;
    }

    if (observation.macAddress == "00:00:00:00:00:00") {
        return std::nullopt;
    }
    return observation;
}

} // namespace

std::string DhcpPlugin::id() const
{
    return sourceIdDhcp;
}

ParserMatch DhcpPlugin::match(const PacketContext& context) const
{
    if (context.udp.has_value() && isDhcpPortPair(*context.udp)) {
        return {100};
    }
    return {};
}

std::vector<AssetObservation> DhcpPlugin::parse(const PacketContext& context) const
{
    if (!match(context).matched() || !context.udp.has_value()) {
        return {};
    }

    const auto observation = parseDhcpPayload(context.udp->payload, context.timestamp, *context.udp);
    if (!observation.has_value()) {
        return {};
    }
    return {*observation};
}

} // namespace asset_discovery::parser
