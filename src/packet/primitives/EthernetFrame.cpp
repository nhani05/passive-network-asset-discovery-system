#include "pnad/packet/EthernetFrame.hpp"

#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

namespace asset_discovery::parser {
namespace {

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

std::uint16_t readBigEndianUInt16(ByteView bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
                                      | static_cast<std::uint16_t>(bytes[offset + 1]));
}

} // namespace

bool EthernetDecodeResult::ok() const
{
    return frame.has_value() && !error.has_value();
}

EthernetDecodeResult decodeEthernetFrame(ByteView bytes)
{
    if (bytes.size < ethernetHeaderLength) {
        return {std::nullopt, EthernetDecodeError::TruncatedFrame};
    }

    EthernetFrame frame;
    frame.destinationMac = formatMacAddress(bytes, 0);
    frame.sourceMac = formatMacAddress(bytes, 6);
    frame.etherType = readBigEndianUInt16(bytes, 12);
    frame.payload = bytes.subview(ethernetHeaderLength);

    return {std::move(frame), std::nullopt};
}

EthernetDecodeResult decodeEthernetFrame(const std::vector<std::uint8_t>& bytes)
{
    return decodeEthernetFrame(makeByteView(bytes));
}

std::string ethernetDecodeErrorName(EthernetDecodeError error)
{
    switch (error) {
    case EthernetDecodeError::TruncatedFrame:
        return "truncated Ethernet frame";
    }
    return "unknown Ethernet decode error";
}

} // namespace asset_discovery::parser
