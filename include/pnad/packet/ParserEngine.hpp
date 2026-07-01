#pragma once

#include "pnad/system/ByteView.hpp"
#include "pnad/packet/ParserRegistry.hpp"

#include <cstdint>
#include <vector>

namespace asset_discovery::parser {

class ParserEngine {
public:
    explicit ParserEngine(ParserRegistry registry);

    std::vector<AssetObservation> parse(const PacketContext& context) const;
    std::vector<AssetObservation> parse(
        ByteView bytes,
        ObservationTimestamp timestamp) const;
    std::vector<AssetObservation> parse(
        const std::vector<std::uint8_t>& bytes,
        ObservationTimestamp timestamp) const;

private:
    ParserRegistry registry_;
};

} // namespace asset_discovery::parser
