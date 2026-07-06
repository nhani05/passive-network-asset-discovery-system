#include "pnad/discovery/AssetMonitor.hpp"

#include <utility>

namespace asset_discovery::monitor {
namespace {

asset::AssetEvent newAssetEvent(const asset::Asset& asset, const AssetMonitorConfig& config)
{
    asset::AssetEvent event;
    event.timestamp = asset.firstSeen;
    event.type = asset::AssetEventType::NewAsset;
    event.severity = asset::AssetEventSeverity::Info;
    if (!asset.ipAddresses.empty()) {
        event.ipAddress = *asset.ipAddresses.begin();
    }
    event.macAddress = asset.macAddress;
    event.hostname = asset.hostname;
    if (!asset.sources.empty()) {
        event.protocol = *asset.sources.begin();
    }
    event.interfaceName = config.interfaceName;
    event.message = "New asset discovered";
    return event;
}

} // namespace

AssetMonitor::AssetMonitor(
    AssetMonitorConfig config,
    EventCallback eventCallback,
    AssetCallback assetCallback)
    : config_(std::move(config))
    , eventCallback_(std::move(eventCallback))
    , assetCallback_(std::move(assetCallback))
{
}

void AssetMonitor::applyObservation(const parser::AssetObservation& observation)
{
    const auto assetCountBefore = store_.size();
    store_.applyObservation(observation);
    const bool isNew = store_.size() > assetCountBefore;

    if (assetCallback_) {
        const auto asset = store_.findByMacAddress(observation.macAddress);
        if (asset.has_value()) {
            assetCallback_(*asset, isNew);
        }
    }

    if (!eventCallback_ || !isNew) {
        return;
    }

    const auto asset = store_.findByMacAddress(observation.macAddress);
    if (asset.has_value()) {
        eventCallback_(newAssetEvent(*asset, config_));
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
