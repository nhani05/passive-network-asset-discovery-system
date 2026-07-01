#include "pnad/discovery/TableRenderer.hpp"

#include <iomanip>
#include <sstream>

namespace asset_discovery::output {
namespace {

std::string joinIpAddresses(const std::set<std::string>& ipAddresses)
{
    std::string output;
    for (const auto& ipAddress : ipAddresses) {
        if (!output.empty()) {
            output += ",";
        }
        output += ipAddress;
    }
    return output;
}

std::string joinSources(const std::set<std::string>& sources)
{
    std::string output;
    for (const auto& source : sources) {
        if (!output.empty()) {
            output += ",";
        }
        output += source;
    }
    return output;
}

} // namespace

std::string renderAssetTable(const std::vector<asset::Asset>& assets)
{
    if (assets.empty()) {
        return "No assets discovered.\n";
    }

    std::ostringstream output;
    output << std::left
           << std::setw(19) << "MAC"
           << std::setw(24) << "IPs"
           << std::setw(18) << "Hostname"
           << std::setw(18) << "First Seen"
           << std::setw(18) << "Last Seen"
           << "Sources\n";
    output << std::string(104, '-') << "\n";

    for (const auto& asset : assets) {
        output << std::left
               << std::setw(19) << asset.macAddress
               << std::setw(24) << joinIpAddresses(asset.ipAddresses)
               << std::setw(18) << asset.hostname.value_or("")
               << std::setw(18) << asset::formatTimestamp(asset.firstSeen)
               << std::setw(18) << asset::formatTimestamp(asset.lastSeen)
               << joinSources(asset.sources) << "\n";
    }

    return output.str();
}

} // namespace asset_discovery::output
