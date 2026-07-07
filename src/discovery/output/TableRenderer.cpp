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
           << std::setw(18) << "Name"
           << std::setw(18) << "Vendor"
           << std::setw(14) << "OS"
           << std::setw(14) << "Type"
           << std::setw(20) << "Model"
           << std::setw(18) << "First Seen"
           << std::setw(18) << "Last Seen"
           << "Sources\n";
    output << std::string(170, '-') << "\n";

    for (const auto& asset : assets) {
        output << std::left
               << std::setw(19) << asset.macAddress
               << std::setw(24) << joinIpAddresses(asset.ipAddresses)
               << std::setw(18) << asset.displayName.value_or(asset.hostname.value_or(""))
               << std::setw(18) << asset.vendor.value_or("")
               << std::setw(14) << asset.osHint.value_or("")
               << std::setw(14) << asset.deviceType.value_or("")
               << std::setw(20) << asset.modelHint.value_or("")
               << std::setw(18) << asset::formatTimestamp(asset.firstSeen)
               << std::setw(18) << asset::formatTimestamp(asset.lastSeen)
               << joinSources(asset.sources) << "\n";
    }

    return output.str();
}

} // namespace asset_discovery::output
