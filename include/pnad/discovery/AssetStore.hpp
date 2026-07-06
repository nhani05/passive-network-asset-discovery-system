#pragma once

#include "pnad/discovery/AssetObservation.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace asset_discovery::asset {

using parser::ObservationTimestamp;

struct Asset {
    std::string macAddress;
    std::set<std::string> ipAddresses;
    std::optional<std::string> hostname;
    std::optional<std::string> displayName;
    std::optional<std::string> vendor;
    std::optional<std::string> osHint;
    std::optional<std::string> deviceType;
    std::optional<std::string> modelHint;
    ObservationTimestamp firstSeen;
    ObservationTimestamp lastSeen;
    std::set<std::string> sources;
    std::map<std::string, std::string> metadata;
    parser::StructuredMetadata structuredMetadata;
};

bool timestampLess(const ObservationTimestamp& left, const ObservationTimestamp& right);
std::string formatTimestamp(const ObservationTimestamp& timestamp);

class AssetStore {
public:
    void applyObservation(const parser::AssetObservation& observation);
    std::optional<Asset> findByMacAddress(const std::string& macAddress) const;
    std::vector<Asset> assets() const;
    std::size_t size() const;

private:
    std::map<std::string, Asset> assetsByMac_;
};

} // namespace asset_discovery::asset
