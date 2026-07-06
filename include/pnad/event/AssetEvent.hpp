#pragma once

#include "pnad/discovery/AssetObservation.hpp"

#include <map>
#include <optional>
#include <string>

namespace asset_discovery::asset {

enum class AssetEventType {
    NewAsset,
};

enum class AssetEventSeverity {
    Info,
};

struct AssetEvent {
    parser::ObservationTimestamp timestamp;
    AssetEventType type = AssetEventType::NewAsset;
    AssetEventSeverity severity = AssetEventSeverity::Info;
    std::optional<std::string> ipAddress;
    std::optional<std::string> macAddress;
    std::optional<std::string> hostname;
    std::string protocol;
    std::string interfaceName;
    std::string message;
    std::map<std::string, std::string> metadata;
};

std::string assetEventTypeName(AssetEventType type);
std::string assetEventSeverityName(AssetEventSeverity severity);
std::string assetEventSeverityLabel(AssetEventSeverity severity);
std::string formatEventTimestamp(const parser::ObservationTimestamp& timestamp);

} // namespace asset_discovery::asset
