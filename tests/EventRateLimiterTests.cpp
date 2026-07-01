#include "domain/EventRateLimiter.hpp"

#include <iostream>
#include <string>

namespace {

using asset_discovery::asset::AssetEvent;
using asset_discovery::asset::AssetEventType;
using asset_discovery::asset::EventRateLimiter;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

AssetEvent event(AssetEventType type, std::int64_t seconds)
{
    AssetEvent result;
    result.type = type;
    result.timestamp = {seconds, 0};
    result.ipAddress = "192.168.1.12";
    result.macAddress = "aa:bb:cc:dd:ee:ff";
    return result;
}

void suppressesDuplicateLowValueEvents()
{
    EventRateLimiter limiter(60);

    expect(limiter.shouldEmit(event(AssetEventType::AssetReappeared, 1)), "first event should emit");
    expect(!limiter.shouldEmit(event(AssetEventType::AssetReappeared, 30)), "duplicate inside window should suppress");
    expect(limiter.shouldEmit(event(AssetEventType::AssetReappeared, 70)), "duplicate after window should emit");
}

void mappingChangesBypassLimiter()
{
    EventRateLimiter limiter(60);

    expect(limiter.shouldEmit(event(AssetEventType::MacChangedForIp, 1)), "first change should emit");
    expect(limiter.shouldEmit(event(AssetEventType::MacChangedForIp, 2)), "second change should bypass limiter");
}

} // namespace

int main()
{
    suppressesDuplicateLowValueEvents();
    mappingChangesBypassLimiter();

    if (failures > 0) {
        std::cerr << failures << " event rate limiter test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
