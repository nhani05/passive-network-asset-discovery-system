#include "application/parser/PacketContext.hpp"

#include <sstream>
#include <utility>
#include <vector>

namespace asset_discovery::parser {
namespace {

std::uint16_t readBigEndianUInt16(ByteView bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
                                      | static_cast<std::uint16_t>(bytes[offset + 1]));
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

std::optional<Ipv4Packet> decodeIpv4Packet(ByteView bytes)
{
    if (bytes.size < 20) {
        return std::nullopt;
    }

    const std::uint8_t version = static_cast<std::uint8_t>(bytes[0] >> 4U);
    const std::size_t headerLength = static_cast<std::size_t>(bytes[0] & 0x0fU) * 4U;
    if (version != 4 || headerLength < 20 || bytes.size < headerLength) {
        return std::nullopt;
    }

    const std::uint16_t totalLength = readBigEndianUInt16(bytes, 2);
    if (totalLength < headerLength || bytes.size < totalLength) {
        return std::nullopt;
    }

    Ipv4Packet packet;
    packet.sourceIp = formatIpv4Address(bytes, 12);
    packet.destinationIp = formatIpv4Address(bytes, 16);
    packet.protocol = bytes[9];
    packet.headerLength = headerLength;
    packet.totalLength = totalLength;
    packet.fragmented = (readBigEndianUInt16(bytes, 6) & 0x3fffU) != 0;
    packet.payload = bytes.subview(headerLength, totalLength - headerLength);
    return packet;
}

std::optional<UdpDatagram> decodeUdpDatagram(const Ipv4Packet& ipv4)
{
    if (ipv4.protocol != ipv4ProtocolUdp || ipv4.fragmented || ipv4.payload.size < udpHeaderLength) {
        return std::nullopt;
    }

    UdpDatagram datagram;
    datagram.sourcePort = readBigEndianUInt16(ipv4.payload, 0);
    datagram.destinationPort = readBigEndianUInt16(ipv4.payload, 2);
    datagram.length = readBigEndianUInt16(ipv4.payload, 4);
    if (datagram.length < udpHeaderLength || datagram.length > ipv4.payload.size) {
        return std::nullopt;
    }

    datagram.payload = ipv4.payload.subview(udpHeaderLength, datagram.length - udpHeaderLength);
    return datagram;
}

} // namespace

PacketContext buildPacketContext(
    ByteView bytes,
    ObservationTimestamp timestamp)
{
    PacketContext context;
    context.rawBytes = bytes;
    context.timestamp = timestamp;

    const auto ethernet = decodeEthernetFrame(bytes);
    if (!ethernet.ok() || !ethernet.frame.has_value()) {
        return context;
    }
    context.ethernet = std::move(*ethernet.frame);

    if (context.ethernet->etherType != ethernetTypeIpv4) {
        return context;
    }

    const auto ipv4 = decodeIpv4Packet(context.ethernet->payload);
    if (!ipv4.has_value()) {
        return context;
    }
    context.ipv4 = std::move(*ipv4);

    const auto udp = decodeUdpDatagram(*context.ipv4);
    if (!udp.has_value()) {
        return context;
    }
    context.udp = std::move(*udp);
    context.transportPayload = context.udp->payload;
    return context;
}

PacketContext buildPacketContext(
    const std::vector<std::uint8_t>& bytes,
    ObservationTimestamp timestamp)
{
    return buildPacketContext(makeByteView(bytes), timestamp);
}

} // namespace asset_discovery::parser
