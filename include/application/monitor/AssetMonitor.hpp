#pragma once

#include "domain/AssetEventDetector.hpp"
#include "domain/AssetStore.hpp"
#include "domain/EventRateLimiter.hpp"

#include <cstddef>
#include <functional>
#include <vector>

namespace asset_discovery::monitor {

struct AssetMonitorConfig {
    asset::AssetEventDetectorConfig detector;
    std::int64_t eventRateLimitSeconds = 60;
};

class AssetMonitor {
public:
    using EventCallback = std::function<void(const asset::AssetEvent&)>;

    explicit AssetMonitor(AssetMonitorConfig config = {}, EventCallback callback = {});

    void applyObservation(const parser::AssetObservation& observation);
    std::vector<asset::Asset> assets() const;
    std::size_t assetCount() const;

private:
    asset::AssetStore store_;
    asset::AssetEventDetector detector_;
    asset::EventRateLimiter rateLimiter_;
    EventCallback callback_;
};

} // namespace asset_discovery::monitor
