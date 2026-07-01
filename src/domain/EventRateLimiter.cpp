#include "domain/EventRateLimiter.hpp"

#include "domain/AssetEventDetector.hpp"

namespace asset_discovery::asset {

EventRateLimiter::EventRateLimiter(std::int64_t windowSeconds)
    : windowSeconds_(windowSeconds)
{
}

bool EventRateLimiter::bypassesRateLimit(AssetEventType type) const
{
    switch (type) {
    case AssetEventType::MacChangedForIp:
    case AssetEventType::IpChangedForMac:
    case AssetEventType::IpMacFlipFlop:
    case AssetEventType::EthernetArpMacMismatch:
    case AssetEventType::NonLocalSourceIp:
    case AssetEventType::HostnameChanged:
        return true;
    case AssetEventType::NewAsset:
    case AssetEventType::AssetSeen:
    case AssetEventType::AssetReappeared:
    case AssetEventType::HostnameLearned:
        return false;
    }
    return false;
}

std::string EventRateLimiter::keyFor(const AssetEvent& event) const
{
    return assetEventTypeName(event.type) + "|"
        + event.ipAddress.value_or("") + "|"
        + event.macAddress.value_or("");
}

bool EventRateLimiter::shouldEmit(const AssetEvent& event)
{
    if (windowSeconds_ <= 0 || bypassesRateLimit(event.type)) {
        return true;
    }

    const auto key = keyFor(event);
    const auto recent = recent_.find(key);
    if (recent == recent_.end()) {
        recent_[key] = event.timestamp;
        return true;
    }

    if (secondsBetween(recent->second, event.timestamp) >= windowSeconds_) {
        recent_[key] = event.timestamp;
        return true;
    }
    return false;
}

} // namespace asset_discovery::asset
