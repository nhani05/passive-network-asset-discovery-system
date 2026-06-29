#pragma once

#include "domain/AssetObservation.hpp"
#include "infrastructure/packet/EthernetFrame.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::parser {

inline constexpr std::uint16_t ethernetTypeIpv4 = 0x0800;
inline constexpr std::uint8_t ipv4ProtocolUdp = 17;
inline constexpr std::size_t udpHeaderLength = 8;

struct Ipv4Packet {
    std::string sourceIp;
    std::string destinationIp;
    std::uint8_t protocol = 0;
    std::size_t headerLength = 0;
    std::uint16_t totalLength = 0;
    bool fragmented = false;
    std::vector<std::uint8_t> payload;
};

struct UdpDatagram {
    std::uint16_t sourcePort = 0;
    std::uint16_t destinationPort = 0;
    std::uint16_t length = 0;
    std::vector<std::uint8_t> payload;
};

struct PacketContext {
    std::vector<std::uint8_t> rawBytes;
    ObservationTimestamp timestamp;
    std::optional<EthernetFrame> ethernet;
    std::optional<Ipv4Packet> ipv4;
    std::optional<UdpDatagram> udp;
    std::vector<std::uint8_t> transportPayload;
};

PacketContext buildPacketContext(
    const std::vector<std::uint8_t>& bytes,
    ObservationTimestamp timestamp);

} // namespace asset_discovery::parser
