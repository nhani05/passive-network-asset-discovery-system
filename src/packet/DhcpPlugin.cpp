#include "pnad/packet/DhcpPlugin.hpp"

#include <iomanip>
#include <optional>
#include <set>
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

std::string formatHexBytes(ByteView bytes, std::size_t offset, std::size_t length)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < length; ++i) {
        if (i > 0) {
            output << ':';
        }
        output << std::setw(2) << static_cast<int>(bytes[offset + i]);
    }
    return output.str();
}

std::string formatDecimalList(ByteView bytes, std::size_t offset, std::size_t length)
{
    std::ostringstream output;
    for (std::size_t i = 0; i < length; ++i) {
        if (i > 0) {
            output << ',';
        }
        output << static_cast<int>(bytes[offset + i]);
    }
    return output.str();
}

std::string joinOptions(const std::set<int>& options)
{
    std::ostringstream output;
    bool first = true;
    for (const auto option : options) {
        if (!first) {
            output << ',';
        }
        output << option;
        first = false;
    }
    return output.str();
}

std::string messageTypeName(std::uint8_t messageType)
{
    switch (messageType) {
    case 1:
        return "discover";
    case 2:
        return "offer";
    case 3:
        return "request";
    case 4:
        return "decline";
    case 5:
        return "ack";
    case 6:
        return "nak";
    case 7:
        return "release";
    case 8:
        return "inform";
    default:
        return "unknown";
    }
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
    addObservedMetadata(observation, "udp.source_port", std::to_string(udp.sourcePort));
    addObservedMetadata(observation, "udp.destination_port", std::to_string(udp.destinationPort));

    const auto yiaddr = formatIpv4Address(payload, 16);
    const auto ciaddr = formatIpv4Address(payload, 12);
    addObservedMetadata(observation, "dhcp.ciaddr", ciaddr);
    addObservedMetadata(observation, "dhcp.yiaddr", yiaddr);
    if (yiaddr != "0.0.0.0") {
        observation.ipAddress = yiaddr;
    } else if (ciaddr != "0.0.0.0") {
        observation.ipAddress = ciaddr;
    }

    std::set<int> optionsSeen;
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
        optionsSeen.insert(static_cast<int>(option));
        if (option == 12) {
            observation.hostname = std::string(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                payload.begin() + static_cast<std::ptrdiff_t>(offset + length));
            addObservedMetadata(observation, "dhcp.option.hostname", *observation.hostname);
        } else if (option == 50 && length == 4 && !observation.ipAddress.has_value()) {
            const auto requestedIp = formatIpv4Address(payload, offset);
            observation.ipAddress = requestedIp;
            addObservedMetadata(observation, "dhcp.requested_ip", requestedIp);
        } else if (option == 50 && length == 4) {
            addObservedMetadata(observation, "dhcp.requested_ip", formatIpv4Address(payload, offset));
        } else if (option == 53 && length == 1) {
            addObservedMetadata(observation, "dhcp.message_type", std::to_string(payload[offset]));
            addObservedMetadata(observation, "dhcp.message_type_name", messageTypeName(payload[offset]));
        } else if (option == 54 && length == 4) {
            addObservedMetadata(observation, "dhcp.server_identifier", formatIpv4Address(payload, offset));
        } else if (option == 51 && length == 4) {
            addObservedMetadata(observation, "dhcp.lease_time_seconds", std::to_string(readBigEndianUInt32(payload, offset)));
        } else if (option == 58 && length == 4) {
            addObservedMetadata(observation, "dhcp.renewal_time_seconds", std::to_string(readBigEndianUInt32(payload, offset)));
        } else if (option == 59 && length == 4) {
            addObservedMetadata(observation, "dhcp.rebinding_time_seconds", std::to_string(readBigEndianUInt32(payload, offset)));
        } else if (option == 60) {
            addObservedMetadata(observation,
                "dhcp.vendor_class_identifier",
                std::string(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                    payload.begin() + static_cast<std::ptrdiff_t>(offset + length)));
        } else if (option == 61) {
            addObservedMetadata(observation, "dhcp.client_identifier", formatHexBytes(payload, offset, length));
        } else if (option == 55) {
            addObservedMetadata(observation, "dhcp.parameter_request_list", formatDecimalList(payload, offset, length));
        }
        offset += length;
    }
    if (!optionsSeen.empty()) {
        addObservedMetadata(observation, "dhcp.options_seen", joinOptions(optionsSeen));
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
