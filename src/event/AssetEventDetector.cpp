#include "pnad/event/AssetEventDetector.hpp"

#include "pnad/discovery/AssetStore.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace asset_discovery::asset {

std::string normalizeMacAddress(std::string macAddress)
{
    std::transform(macAddress.begin(), macAddress.end(), macAddress.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return macAddress;
}

AssetEventDetector::AssetEventDetector(AssetEventDetectorConfig config)
    : config_(std::move(config))
{
}

AssetEvent AssetEventDetector::baseEvent(
    const parser::AssetObservation& observation,
    AssetEventType type,
    AssetEventSeverity severity,
    std::string message) const
{
    AssetEvent event;
    event.timestamp = observation.timestamp;
    event.type = type;
    event.severity = severity;
    event.ipAddress = observation.ipAddress;
    event.macAddress = normalizeMacAddress(observation.macAddress);
    event.hostname = observation.hostname;
    event.protocol = observation.sourceId;
    event.interfaceName = config_.interfaceName;
    event.message = std::move(message);
    return event;
}

void AssetEventDetector::rememberObservation(
    const parser::AssetObservation& observation,
    const std::string& macAddress)
{
    auto known = knownMacs_.find(macAddress);
    if (known == knownMacs_.end()) {
        knownMacs_[macAddress] = {observation.timestamp, observation.timestamp};
    } else {
        if (timestampLess(observation.timestamp, known->second.firstSeen)) {
            known->second.firstSeen = observation.timestamp;
        }
        if (timestampLess(known->second.lastSeen, observation.timestamp)) {
            known->second.lastSeen = observation.timestamp;
        }
    }

    if (observation.ipAddress.has_value()) {
        (void)macAddress;
    }
}

std::vector<AssetEvent> AssetEventDetector::detectAndRemember(
    const parser::AssetObservation& observation)
{
    std::vector<AssetEvent> events;
    if (observation.macAddress.empty()) {
        return events;
    }

    const auto macAddress = normalizeMacAddress(observation.macAddress);
    const auto knownMac = knownMacs_.find(macAddress);
    const bool isNewMac = knownMac == knownMacs_.end();

    if (isNewMac) {
        events.push_back(baseEvent(
            observation,
            AssetEventType::NewAsset,
            AssetEventSeverity::Info,
            "New asset discovered"));
    }

    rememberObservation(observation, macAddress);
    return events;
}

} // namespace asset_discovery::asset
