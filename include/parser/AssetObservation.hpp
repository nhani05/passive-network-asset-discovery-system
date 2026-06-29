#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace asset_discovery::parser {

struct ObservationTimestamp {
    std::int64_t seconds = 0;
    std::int64_t microseconds = 0;
};

enum class ObservationSource {
    Arp,
    Dhcp,
};

struct AssetObservation {
    std::string macAddress;
    std::optional<std::string> ipAddress;
    std::optional<std::string> hostname;
    ObservationSource source = ObservationSource::Arp;
    ObservationTimestamp timestamp;
};

std::string observationSourceName(ObservationSource source);

} // namespace asset_discovery::parser
