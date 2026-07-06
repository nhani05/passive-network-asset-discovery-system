#pragma once

#include "pnad/event/AssetEvent.hpp"

#include <map>
#include <string>
#include <vector>

namespace asset_discovery::asset {

struct AssetEventDetectorConfig {
    std::string interfaceName;
};

class AssetEventDetector {
public:
    explicit AssetEventDetector(AssetEventDetectorConfig config = {});

    std::vector<AssetEvent> detectAndRemember(const parser::AssetObservation& observation);

private:
    struct MacState {
        parser::ObservationTimestamp firstSeen;
        parser::ObservationTimestamp lastSeen;
    };

    AssetEvent baseEvent(
        const parser::AssetObservation& observation,
        AssetEventType type,
        AssetEventSeverity severity,
        std::string message) const;

    void rememberObservation(const parser::AssetObservation& observation, const std::string& macAddress);

    AssetEventDetectorConfig config_;
    std::map<std::string, MacState> knownMacs_;
};

std::string normalizeMacAddress(std::string macAddress);

} // namespace asset_discovery::asset
