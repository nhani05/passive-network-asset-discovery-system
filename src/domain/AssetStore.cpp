#include "domain/AssetStore.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace asset_discovery::asset {
namespace {

std::string normalizeMacAddress(std::string macAddress)
{
    std::transform(macAddress.begin(), macAddress.end(), macAddress.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return macAddress;
}

} // namespace

bool timestampLess(const ObservationTimestamp& left, const ObservationTimestamp& right)
{
    if (left.seconds != right.seconds) {
        return left.seconds < right.seconds;
    }
    return left.microseconds < right.microseconds;
}

std::string formatTimestamp(const ObservationTimestamp& timestamp)
{
    std::ostringstream output;
    output << timestamp.seconds << "." << timestamp.microseconds;
    return output.str();
}

void AssetStore::applyObservation(const parser::AssetObservation& observation)
{
    if (observation.macAddress.empty()) {
        return;
    }

    const auto macAddress = normalizeMacAddress(observation.macAddress);
    auto existing = assetsByMac_.find(macAddress);
    if (existing == assetsByMac_.end()) {
        Asset asset;
        asset.macAddress = macAddress;
        asset.firstSeen = observation.timestamp;
        asset.lastSeen = observation.timestamp;
        if (observation.ipAddress.has_value()) {
            asset.ipAddresses.insert(*observation.ipAddress);
        }
        if (observation.hostname.has_value() && !observation.hostname->empty()) {
            asset.hostname = *observation.hostname;
        }
        if (!observation.sourceId.empty()) {
            asset.sources.insert(observation.sourceId);
        }
        asset.metadata = observation.metadata;
        assetsByMac_.emplace(macAddress, std::move(asset));
        return;
    }

    auto& asset = existing->second;
    if (timestampLess(observation.timestamp, asset.firstSeen)) {
        asset.firstSeen = observation.timestamp;
    }
    if (timestampLess(asset.lastSeen, observation.timestamp)) {
        asset.lastSeen = observation.timestamp;
    }
    if (observation.ipAddress.has_value()) {
        asset.ipAddresses.insert(*observation.ipAddress);
    }
    if (observation.hostname.has_value() && !observation.hostname->empty()) {
        asset.hostname = *observation.hostname;
    }
    if (!observation.sourceId.empty()) {
        asset.sources.insert(observation.sourceId);
    }
    for (const auto& metadata : observation.metadata) {
        asset.metadata[metadata.first] = metadata.second;
    }
}

std::vector<Asset> AssetStore::assets() const
{
    std::vector<Asset> result;
    result.reserve(assetsByMac_.size());
    for (const auto& item : assetsByMac_) {
        result.push_back(item.second);
    }
    return result;
}

std::size_t AssetStore::size() const
{
    return assetsByMac_.size();
}

} // namespace asset_discovery::asset
