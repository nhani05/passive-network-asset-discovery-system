#pragma once

#include "pnad/system/ByteView.hpp"
#include "pnad/discovery/AssetObservation.hpp"
#include "pnad/packet/EthernetFrame.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::parser {

inline constexpr std::uint16_t ethernetTypeIpv4 = 0x0800;
inline constexpr std::uint8_t ipv4ProtocolTcp = 6;
inline constexpr std::uint8_t ipv4ProtocolUdp = 17;
inline constexpr std::size_t tcpMinimumHeaderLength = 20;
inline constexpr std::size_t udpHeaderLength = 8;

struct Ipv4Packet {
    std::string sourceIp;
    std::string destinationIp;
    std::uint8_t protocol = 0;
    std::size_t headerLength = 0;
    std::uint16_t totalLength = 0;
    bool fragmented = false;
    ByteView payload;
};

struct UdpDatagram {
    std::uint16_t sourcePort = 0;
    std::uint16_t destinationPort = 0;
    std::uint16_t length = 0;
    ByteView payload;
};

struct TcpSegment {
    std::uint16_t sourcePort = 0;
    std::uint16_t destinationPort = 0;
    std::size_t headerLength = 0;
    bool syn = false;
    bool ack = false;
    bool rst = false;
    bool fin = false;
    ByteView payload;
};

struct PacketContext {
    ByteView rawBytes;
    ObservationTimestamp timestamp;
    std::optional<EthernetFrame> ethernet;
    std::optional<Ipv4Packet> ipv4;
    std::optional<UdpDatagram> udp;
    std::optional<TcpSegment> tcp;
    ByteView transportPayload;
};

PacketContext buildPacketContext(
    ByteView bytes,
    ObservationTimestamp timestamp);

PacketContext buildPacketContext(
    const std::vector<std::uint8_t>& bytes,
    ObservationTimestamp timestamp);

} // namespace asset_discovery::parser
