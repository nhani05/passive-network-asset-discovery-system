#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace asset_discovery::parser {

struct ObservationTimestamp {
    std::int64_t seconds = 0;
    std::int64_t microseconds = 0;
};

enum class ObservationEventType {
    Seen,
    Update,
    Discovery,
};

inline constexpr const char* sourceIdArp = "arp";
inline constexpr const char* sourceIdDhcp = "dhcp";
inline constexpr const char* sourceIdDns = "dns";

struct AssetObservation {
    std::string macAddress;
    std::optional<std::string> ipAddress;
    std::optional<std::string> hostname;
    std::string sourceId = sourceIdArp;
    ObservationEventType eventType = ObservationEventType::Seen;
    float confidence = 1.0F;
    std::map<std::string, std::string> metadata;
    ObservationTimestamp timestamp;
};

std::string observationEventTypeName(ObservationEventType eventType);

} // namespace asset_discovery::parser
