#include "parser/PacketParsers.hpp"

#include "parser/ArpPacket.hpp"
#include "parser/EthernetFrame.hpp"

#include <iomanip>
#include <sstream>

namespace asset_discovery::parser {
namespace {

constexpr std::uint16_t ethernetTypeIpv4 = 0x0800;
constexpr std::uint8_t ipv4ProtocolUdp = 17;
constexpr std::uint16_t dhcpServerPort = 67;
constexpr std::uint16_t dhcpClientPort = 68;
constexpr std::size_t udpHeaderLength = 8;
constexpr std::size_t dhcpFixedHeaderLength = 236;
constexpr std::uint32_t dhcpMagicCookie = 0x63825363;

std::uint16_t readBigEndianUInt16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
                                      | static_cast<std::uint16_t>(bytes[offset + 1]));
}

std::uint32_t readBigEndianUInt32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U)
        | static_cast<std::uint32_t>(bytes[offset + 3]);
}

std::string formatMacAddress(const std::vector<std::uint8_t>& bytes, std::size_t offset)
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

std::string formatIpv4Address(const std::vector<std::uint8_t>& bytes, std::size_t offset)
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
    const std::vector<std::uint8_t>& payload,
    ObservationTimestamp timestamp)
{
    if (payload.size() < dhcpFixedHeaderLength + 4) {
        return std::nullopt;
    }
    if (readBigEndianUInt32(payload, dhcpFixedHeaderLength) != dhcpMagicCookie) {
        return std::nullopt;
    }

    AssetObservation observation;
    observation.macAddress = formatMacAddress(payload, 28);
    observation.source = ObservationSource::Dhcp;
    observation.timestamp = timestamp;

    const auto yiaddr = formatIpv4Address(payload, 16);
    const auto ciaddr = formatIpv4Address(payload, 12);
    if (yiaddr != "0.0.0.0") {
        observation.ipAddress = yiaddr;
    } else if (ciaddr != "0.0.0.0") {
        observation.ipAddress = ciaddr;
    }

    std::size_t offset = dhcpFixedHeaderLength + 4;
    while (offset < payload.size()) {
        const std::uint8_t option = payload[offset++];
        if (option == 255) {
            break;
        }
        if (option == 0) {
            continue;
        }
        if (offset >= payload.size()) {
            return std::nullopt;
        }
        const std::size_t length = payload[offset++];
        if (payload.size() - offset < length) {
            return std::nullopt;
        }
        if (option == 12) {
            observation.hostname = std::string(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                payload.begin() + static_cast<std::ptrdiff_t>(offset + length));
        } else if (option == 50 && length == 4 && !observation.ipAddress.has_value()) {
            observation.ipAddress = formatIpv4Address(payload, offset);
        }
        offset += length;
    }

    if (observation.macAddress == "00:00:00:00:00:00") {
        return std::nullopt;
    }
    return observation;
}

std::vector<AssetObservation> parseIpv4UdpObservations(
    const std::vector<std::uint8_t>& payload,
    ObservationTimestamp timestamp)
{
    if (payload.size() < 20) {
        return {};
    }
    const std::uint8_t version = static_cast<std::uint8_t>(payload[0] >> 4U);
    const std::size_t headerLength = static_cast<std::size_t>(payload[0] & 0x0fU) * 4U;
    if (version != 4 || headerLength < 20 || payload.size() < headerLength) {
        return {};
    }
    const std::uint16_t totalLength = readBigEndianUInt16(payload, 2);
    if (totalLength < headerLength || payload.size() < totalLength) {
        return {};
    }
    const std::uint16_t fragment = readBigEndianUInt16(payload, 6);
    if ((fragment & 0x3fffU) != 0) {
        return {};
    }
    if (payload[9] != ipv4ProtocolUdp) {
        return {};
    }
    if (totalLength - headerLength < udpHeaderLength) {
        return {};
    }

    const std::size_t udpOffset = headerLength;
    const std::uint16_t sourcePort = readBigEndianUInt16(payload, udpOffset);
    const std::uint16_t destinationPort = readBigEndianUInt16(payload, udpOffset + 2);
    const std::uint16_t udpLength = readBigEndianUInt16(payload, udpOffset + 4);
    if (udpLength < udpHeaderLength || udpOffset + udpLength > totalLength) {
        return {};
    }
    const bool isDhcp = (sourcePort == dhcpServerPort || sourcePort == dhcpClientPort)
        && (destinationPort == dhcpServerPort || destinationPort == dhcpClientPort);
    if (!isDhcp) {
        return {};
    }

    std::vector<std::uint8_t> dhcpPayload(
        payload.begin() + static_cast<std::ptrdiff_t>(udpOffset + udpHeaderLength),
        payload.begin() + static_cast<std::ptrdiff_t>(udpOffset + udpLength));
    const auto observation = parseDhcpPayload(dhcpPayload, timestamp);
    if (!observation.has_value()) {
        return {};
    }
    return {*observation};
}

} // namespace

std::vector<AssetObservation> parseEthernetObservations(
    const std::vector<std::uint8_t>& bytes,
    ObservationTimestamp timestamp)
{
    const auto ethernet = decodeEthernetFrame(bytes);
    if (!ethernet.ok() || !ethernet.frame.has_value()) {
        return {};
    }
    if (ethernet.frame->etherType == ethernetTypeArp) {
        return parseArpObservationsFromEthernetFrame(bytes, timestamp);
    }
    if (ethernet.frame->etherType == ethernetTypeIpv4) {
        return parseIpv4UdpObservations(ethernet.frame->payload, timestamp);
    }
    return {};
}

} // namespace asset_discovery::parser
