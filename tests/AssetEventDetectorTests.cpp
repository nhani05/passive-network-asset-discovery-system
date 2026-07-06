#include "pnad/event/AssetEventDetector.hpp"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using asset_discovery::asset::AssetEventDetector;
using asset_discovery::asset::AssetEventType;
using asset_discovery::asset::assetEventTypeName;
using asset_discovery::parser::AssetObservation;
using asset_discovery::parser::ObservationTimestamp;
using asset_discovery::parser::sourceIdArp;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

AssetObservation observation(
    std::string macAddress,
    std::string ipAddress,
    ObservationTimestamp timestamp = {1, 0})
{
    AssetObservation result;
    result.macAddress = std::move(macAddress);
    result.ipAddress = std::move(ipAddress);
    result.sourceId = sourceIdArp;
    result.timestamp = timestamp;
    return result;
}

bool hasType(const std::vector<asset_discovery::asset::AssetEvent>& events, AssetEventType type)
{
    for (const auto& event : events) {
        if (event.type == type) {
            return true;
        }
    }
    return false;
}

const asset_discovery::asset::AssetEvent* findType(
    const std::vector<asset_discovery::asset::AssetEvent>& events,
    AssetEventType type)
{
    for (const auto& event : events) {
        if (event.type == type) {
            return &event;
        }
    }
    return nullptr;
}

void detectsNewAssetOnce()
{
    AssetEventDetector detector;
    auto events = detector.detectAndRemember(observation("AA:BB:CC:DD:EE:FF", "192.168.1.12"));
    expect(hasType(events, AssetEventType::NewAsset), "first MAC observation should emit new_asset");
    const auto* event = findType(events, AssetEventType::NewAsset);
    if (event != nullptr) {
        expect(assetEventTypeName(event->type) == "new_asset", "event type name should be stable");
        expect(event->macAddress == "aa:bb:cc:dd:ee:ff", "event MAC should be normalized");
        expect(event->ipAddress == "192.168.1.12", "event should include IP");
        expect(event->message == "New asset discovered", "event message should describe asset creation");
    }

    events = detector.detectAndRemember(observation("aa:bb:cc:dd:ee:ff", "192.168.1.40", {2, 0}));
    expect(!hasType(events, AssetEventType::NewAsset), "repeated MAC should not emit duplicate new_asset");
}

} // namespace

int main()
{
    detectsNewAssetOnce();

    if (failures > 0) {
        std::cerr << failures << " asset event detector test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
