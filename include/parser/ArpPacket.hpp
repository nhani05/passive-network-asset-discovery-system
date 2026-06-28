#pragma once

#include "parser/AssetObservation.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::parser {

constexpr std::size_t arpEthernetIpv4Length = 28;
constexpr std::uint16_t ethernetTypeArp = 0x0806;
constexpr std::uint16_t arpHardwareTypeEthernet = 1;
constexpr std::uint16_t arpProtocolTypeIpv4 = 0x0800;
constexpr std::uint8_t arpHardwareLengthEthernet = 6;
constexpr std::uint8_t arpProtocolLengthIpv4 = 4;

enum class ArpDecodeError {
    TruncatedPacket,
    UnsupportedHardwareType,
    UnsupportedProtocolType,
    UnsupportedHardwareLength,
    UnsupportedProtocolLength,
};

struct ArpPacket {
    std::uint16_t operation = 0;
    std::string senderMac;
    std::string senderIp;
    std::string targetMac;
    std::string targetIp;
};

struct ArpDecodeResult {
    std::optional<ArpPacket> packet;
    std::optional<ArpDecodeError> error;

    bool ok() const;
};

ArpDecodeResult decodeArpPacket(const std::vector<std::uint8_t>& bytes);
AssetObservation observationFromArpPacket(const ArpPacket& packet, ObservationTimestamp timestamp);
std::vector<AssetObservation> parseArpObservationsFromEthernetFrame(
    const std::vector<std::uint8_t>& bytes,
    ObservationTimestamp timestamp);
std::string arpDecodeErrorName(ArpDecodeError error);

} // namespace asset_discovery::parser
