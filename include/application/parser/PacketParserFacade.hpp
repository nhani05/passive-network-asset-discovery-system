#pragma once

#include "domain/ByteView.hpp"
#include "domain/AssetObservation.hpp"

#include <cstdint>
#include <vector>

namespace asset_discovery::parser {

std::vector<AssetObservation> parseEthernetObservations(
    ByteView bytes,
    ObservationTimestamp timestamp);

std::vector<AssetObservation> parseEthernetObservations(
    const std::vector<std::uint8_t>& bytes,
    ObservationTimestamp timestamp);

} // namespace asset_discovery::parser
