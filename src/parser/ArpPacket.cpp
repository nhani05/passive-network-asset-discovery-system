#include "parser/ArpPacket.hpp"

#include "parser/EthernetFrame.hpp"

#include <iomanip>
#include <sstream>
#include <utility>

namespace asset_discovery::parser {
namespace {

std::uint16_t readBigEndianUInt16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
                                      | static_cast<std::uint16_t>(bytes[offset + 1]));
}

std::string formatMacAddress(const std::vector<std::uint8_t>& bytes, std::size_t offset)
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

std::string formatIpv4Address(const std::vector<std::uint8_t>& bytes, std::size_t offset)
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

ArpDecodeResult decodeArpPacket(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() < arpEthernetIpv4Length) {
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

AssetObservation observationFromArpPacket(const ArpPacket& packet, ObservationTimestamp timestamp)
{
    AssetObservation observation;
    observation.macAddress = packet.senderMac;
    observation.ipAddress = packet.senderIp;
    observation.source = ObservationSource::Arp;
    observation.timestamp = timestamp;
    return observation;
}

std::vector<AssetObservation> parseArpObservationsFromEthernetFrame(
    const std::vector<std::uint8_t>& bytes,
    ObservationTimestamp timestamp)
{
    const auto ethernet = decodeEthernetFrame(bytes);
    if (!ethernet.ok() || !ethernet.frame.has_value() || ethernet.frame->etherType != ethernetTypeArp) {
        return {};
    }

    const auto arp = decodeArpPacket(ethernet.frame->payload);
    if (!arp.ok() || !arp.packet.has_value()) {
        return {};
    }

    return {observationFromArpPacket(*arp.packet, timestamp)};
}

std::string arpDecodeErrorName(ArpDecodeError error)
{
    switch (error) {
    case ArpDecodeError::TruncatedPacket:
        return "ARP packet bị thiếu dữ liệu";
    case ArpDecodeError::UnsupportedHardwareType:
        return "loại phần cứng ARP không được hỗ trợ";
    case ArpDecodeError::UnsupportedProtocolType:
        return "loại giao thức ARP không được hỗ trợ";
    case ArpDecodeError::UnsupportedHardwareLength:
        return "độ dài phần cứng ARP không được hỗ trợ";
    case ArpDecodeError::UnsupportedProtocolLength:
        return "độ dài giao thức ARP không được hỗ trợ";
    }
    return "lỗi giải mã ARP không xác định";
}

} // namespace asset_discovery::parser
