#include "pnad/event/AssetEvent.hpp"

#include <sstream>

namespace asset_discovery::asset {

std::string assetEventTypeName(AssetEventType type)
{
    switch (type) {
    case AssetEventType::NewAsset:
        return "new_asset";
    }
    return "new_asset";
}

std::string assetEventSeverityName(AssetEventSeverity severity)
{
    switch (severity) {
    case AssetEventSeverity::Info:
        return "info";
    }
    return "info";
}

std::string assetEventSeverityLabel(AssetEventSeverity severity)
{
    switch (severity) {
    case AssetEventSeverity::Info:
        return "INFO";
    }
    return "INFO";
}

std::string formatEventTimestamp(const parser::ObservationTimestamp& timestamp)
{
    std::ostringstream output;
    output << timestamp.seconds << "." << timestamp.microseconds;
    return output.str();
}

} // namespace asset_discovery::asset
