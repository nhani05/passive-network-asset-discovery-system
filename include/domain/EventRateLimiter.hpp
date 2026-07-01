#pragma once

#include "domain/AssetEvent.hpp"

#include <cstdint>
#include <map>
#include <string>

namespace asset_discovery::asset {

class EventRateLimiter {
public:
    explicit EventRateLimiter(std::int64_t windowSeconds = 60);

    bool shouldEmit(const AssetEvent& event);

private:
    bool bypassesRateLimit(AssetEventType type) const;
    std::string keyFor(const AssetEvent& event) const;

    std::int64_t windowSeconds_;
    std::map<std::string, parser::ObservationTimestamp> recent_;
};

} // namespace asset_discovery::asset
