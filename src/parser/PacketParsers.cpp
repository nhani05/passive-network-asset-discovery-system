#include "parser/PacketParsers.hpp"

#include <iomanip>
#include <sstream>

namespace asset_discovery::parser {
namespace {

constexpr std::size_t ethernetHeaderLength = 14;
constexpr std::size_t arpEthernetIpv4Length = 28;
constexpr std::uint16_t etherTypeArp = 0x0806;
constexpr std::uint16_t arpHardwareTypeEthernet = 1;
constexpr std::uint16_t arpProtocolTypeIpv4 = 0x0800;
constexpr std::uint8_t arpHardwareLengthEthernet = 6;
constexpr std::uint8_t arpProtocolLengthIpv4 = 4;

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

std::vector<AssetObservation> parseArpPayload(
    const std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    ObservationTimestamp timestamp)
{
    if (bytes.size() < offset + arpEthernetIpv4Length) {
        return {};
    }

    const auto hardwareType = readBigEndianUInt16(bytes, offset);
    const auto protocolType = readBigEndianUInt16(bytes, offset + 2);
    if (hardwareType != arpHardwareTypeEthernet
        || protocolType != arpProtocolTypeIpv4
        || bytes[offset + 4] != arpHardwareLengthEthernet
        || bytes[offset + 5] != arpProtocolLengthIpv4) {
        return {};
    }

    AssetObservation observation;
    observation.macAddress = formatMacAddress(bytes, offset + 8);
    observation.ipAddress = formatIpv4Address(bytes, offset + 14);
    observation.source = ObservationSource::Arp;
    observation.timestamp = timestamp;
    return {observation};
}

} // namespace

std::vector<AssetObservation> parseEthernetObservations(
    const std::vector<std::uint8_t>& bytes,
    ObservationTimestamp timestamp)
{
    if (bytes.size() < ethernetHeaderLength) {
        return {};
    }

    const auto etherType = readBigEndianUInt16(bytes, 12);
    if (etherType != etherTypeArp) {
        return {};
    }

    return parseArpPayload(bytes, ethernetHeaderLength, timestamp);
}

} // namespace asset_discovery::parser
