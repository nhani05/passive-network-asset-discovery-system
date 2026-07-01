#include "infrastructure/packet/ArpPacket.hpp"

#include <iomanip>
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

std::string formatMacAddress(ByteView bytes, std::size_t offset)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < arpHardwareLengthEthernet; ++i) {
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
    for (std::size_t i = 0; i < arpProtocolLengthIpv4; ++i) {
        if (i > 0) {
            output << '.';
        }
        output << static_cast<int>(bytes[offset + i]);
    }
    return output.str();
}

} // namespace

bool ArpDecodeResult::ok() const
{
    return packet.has_value() && !error.has_value();
}

ArpDecodeResult decodeArpPacket(ByteView bytes)
{
    if (bytes.size < arpEthernetIpv4Length) {
        return {std::nullopt, ArpDecodeError::TruncatedPacket};
    }

    const auto hardwareType = readBigEndianUInt16(bytes, 0);
    if (hardwareType != arpHardwareTypeEthernet) {
        return {std::nullopt, ArpDecodeError::UnsupportedHardwareType};
    }

    const auto protocolType = readBigEndianUInt16(bytes, 2);
    if (protocolType != arpProtocolTypeIpv4) {
        return {std::nullopt, ArpDecodeError::UnsupportedProtocolType};
    }

    if (bytes[4] != arpHardwareLengthEthernet) {
        return {std::nullopt, ArpDecodeError::UnsupportedHardwareLength};
    }

    if (bytes[5] != arpProtocolLengthIpv4) {
        return {std::nullopt, ArpDecodeError::UnsupportedProtocolLength};
    }

    ArpPacket packet;
    packet.operation = readBigEndianUInt16(bytes, 6);
    packet.senderMac = formatMacAddress(bytes, 8);
    packet.senderIp = formatIpv4Address(bytes, 14);
    packet.targetMac = formatMacAddress(bytes, 18);
    packet.targetIp = formatIpv4Address(bytes, 24);

    return {std::move(packet), std::nullopt};
}

ArpDecodeResult decodeArpPacket(const std::vector<std::uint8_t>& bytes)
{
    return decodeArpPacket(makeByteView(bytes));
}

AssetObservation observationFromArpPacket(const ArpPacket& packet, ObservationTimestamp timestamp)
{
    AssetObservation observation;
    observation.macAddress = packet.senderMac;
    observation.ipAddress = packet.senderIp;
    observation.sourceId = sourceIdArp;
    observation.eventType = ObservationEventType::Seen;
    observation.confidence = 1.0F;
    observation.timestamp = timestamp;
    return observation;
}

std::string arpDecodeErrorName(ArpDecodeError error)
{
    switch (error) {
    case ArpDecodeError::TruncatedPacket:
        return "truncated ARP packet";
    case ArpDecodeError::UnsupportedHardwareType:
        return "unsupported ARP hardware type";
    case ArpDecodeError::UnsupportedProtocolType:
        return "unsupported ARP protocol type";
    case ArpDecodeError::UnsupportedHardwareLength:
        return "unsupported ARP hardware length";
    case ArpDecodeError::UnsupportedProtocolLength:
        return "unsupported ARP protocol length";
    }
    return "unknown ARP decode error";
}

} // namespace asset_discovery::parser
