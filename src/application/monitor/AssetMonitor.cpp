#include "application/monitor/AssetMonitor.hpp"

#include <utility>

namespace asset_discovery::monitor {

AssetMonitor::AssetMonitor(AssetMonitorConfig config, EventCallback callback)
    : detector_(std::move(config.detector))
    , rateLimiter_(config.eventRateLimitSeconds)
    , callback_(std::move(callback))
{
}

void AssetMonitor::applyObservation(const parser::AssetObservation& observation)
{
    auto events = detector_.detectAndRemember(observation);
    store_.applyObservation(observation);

    if (!callback_) {
        return;
    }

    for (const auto& event : events) {
        if (rateLimiter_.shouldEmit(event)) {
            callback_(event);
        }
    }
}

std::vector<asset::Asset> AssetMonitor::assets() const
{
    return store_.assets();
}

std::size_t AssetMonitor::assetCount() const
{
    return store_.size();
}

} // namespace asset_discovery::monitor
