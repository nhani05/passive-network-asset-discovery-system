#pragma once

#include "pnad/discovery/AssetStore.hpp"
#include "pnad/event/AssetEvent.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace asset_discovery::monitor {

struct AssetMonitorConfig {
    std::string interfaceName;
};

class AssetMonitor {
public:
    using EventCallback = std::function<void(const asset::AssetEvent&)>;
    using AssetCallback = std::function<void(const asset::Asset&, bool isNew)>;

    explicit AssetMonitor(
        AssetMonitorConfig config = {},
        EventCallback eventCallback = {},
        AssetCallback assetCallback = {});

    void applyObservation(const parser::AssetObservation& observation);
    std::vector<asset::Asset> assets() const;
    std::size_t assetCount() const;

private:
    asset::AssetStore store_;
    AssetMonitorConfig config_;
    EventCallback eventCallback_;
    AssetCallback assetCallback_;
};

} // namespace asset_discovery::monitor
