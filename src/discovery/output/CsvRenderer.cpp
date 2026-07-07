#include "pnad/discovery/CsvRenderer.hpp"

#include <sstream>

namespace asset_discovery::output {
namespace {

std::string joinStrings(const std::set<std::string>& values)
{
    std::string output;
    for (const auto& value : values) {
        if (!output.empty()) {
            output += ";";
        }
        output += value;
    }
    return output;
}

std::string joinSources(const std::set<std::string>& sources)
{
    std::string output;
    for (const auto& source : sources) {
        if (!output.empty()) {
            output += ";";
        }
        output += source;
    }
    return output;
}

std::string escapeCsvField(const std::string& value)
{
    bool needsQuotes = false;
    for (const char character : value) {
        if (character == '"' || character == ',' || character == '\n' || character == '\r') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) {
        return value;
    }

    std::string output = "\"";
    for (const char character : value) {
        if (character == '"') {
            output += "\"\"";
        } else {
            output += character;
        }
    }
    output += "\"";
    return output;
}

} // namespace

std::string renderAssetCsv(const std::vector<asset::Asset>& assets)
{
    std::ostringstream output;
    output << "mac_address,ip_addresses,hostname,display_name,vendor,os_hint,device_type,model_hint,"
           << "first_seen,last_seen,discovery_sources\n";
    for (const auto& asset : assets) {
        output << escapeCsvField(asset.macAddress) << ","
               << escapeCsvField(joinStrings(asset.ipAddresses)) << ","
               << escapeCsvField(asset.hostname.value_or("")) << ","
               << escapeCsvField(asset.displayName.value_or("")) << ","
               << escapeCsvField(asset.vendor.value_or("")) << ","
               << escapeCsvField(asset.osHint.value_or("")) << ","
               << escapeCsvField(asset.deviceType.value_or("")) << ","
               << escapeCsvField(asset.modelHint.value_or("")) << ","
               << escapeCsvField(asset::formatTimestamp(asset.firstSeen)) << ","
               << escapeCsvField(asset::formatTimestamp(asset.lastSeen)) << ","
               << escapeCsvField(joinSources(asset.sources)) << "\n";
    }
    return output.str();
}

} // namespace asset_discovery::output
