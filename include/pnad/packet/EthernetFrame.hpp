#pragma once

#include "pnad/system/ByteView.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::parser {

constexpr std::size_t ethernetHeaderLength = 14;

enum class EthernetDecodeError {
    TruncatedFrame,
};

struct EthernetFrame {
    std::string destinationMac;
    std::string sourceMac;
    std::uint16_t etherType = 0;
    ByteView payload;
};

struct EthernetDecodeResult {
    std::optional<EthernetFrame> frame;
    std::optional<EthernetDecodeError> error;

    bool ok() const;
};

EthernetDecodeResult decodeEthernetFrame(ByteView bytes);
EthernetDecodeResult decodeEthernetFrame(const std::vector<std::uint8_t>& bytes);
std::string ethernetDecodeErrorName(EthernetDecodeError error);

} // namespace asset_discovery::parser
