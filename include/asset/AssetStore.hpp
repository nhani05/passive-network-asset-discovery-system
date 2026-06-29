#pragma once

#include "parser/AssetObservation.hpp"

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
    ObservationTimestamp firstSeen;
    ObservationTimestamp lastSeen;
    std::set<parser::ObservationSource> sources;
};

bool timestampLess(const ObservationTimestamp& left, const ObservationTimestamp& right);
std::string formatTimestamp(const ObservationTimestamp& timestamp);

class AssetStore {
public:
    void applyObservation(const parser::AssetObservation& observation);
    std::vector<Asset> assets() const;
    std::size_t size() const;

private:
    std::map<std::string, Asset> assetsByMac_;
};

} // namespace asset_discovery::asset
