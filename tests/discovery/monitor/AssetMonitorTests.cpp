#include "pnad/discovery/AssetMonitor.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::asset::AssetEvent;
using asset_discovery::asset::AssetEventType;
using asset_discovery::monitor::AssetMonitor;
using asset_discovery::parser::AssetObservation;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

AssetObservation observation(const std::string& mac, const std::string& ip, std::int64_t seconds)
{
    AssetObservation result;
    result.macAddress = mac;
    result.ipAddress = ip;
    result.timestamp = {seconds, 0};
    return result;
}

void emitsOnlyNewAssetEventsAndPreservesSummary()
{
    std::vector<AssetEvent> events;
    AssetMonitor monitor({}, [&](const AssetEvent& event) {
        events.push_back(event);
    });

    monitor.applyObservation(observation("aa:bb:cc:dd:ee:ff", "192.168.1.12", 1));
    monitor.applyObservation(observation("aa:bb:cc:dd:ee:ff", "192.168.1.40", 2));

    const auto assets = monitor.assets();
    expect(events.size() == 1, "monitor should emit only one new_asset event for one MAC");
    if (!events.empty()) {
        expect(events.front().type == AssetEventType::NewAsset, "monitor should emit new_asset");
        expect(events.front().macAddress == "aa:bb:cc:dd:ee:ff", "event MAC should be normalized");
    }
    expect(assets.size() == 1, "monitor should preserve final asset aggregation");
    if (!assets.empty()) {
        expect(assets.front().ipAddresses.count("192.168.1.12") == 1, "summary should keep old IP");
        expect(assets.front().ipAddresses.count("192.168.1.40") == 1, "summary should keep new IP");
    }
}

void reportsAssetUpdatesImmediately()
{
    std::vector<std::string> callbackMacs;
    std::vector<bool> callbackNewFlags;
    AssetMonitor monitor(
        {},
        {},
        [&](const asset_discovery::asset::Asset& asset, bool isNew) {
            callbackMacs.push_back(asset.macAddress);
            callbackNewFlags.push_back(isNew);
        });

    monitor.applyObservation(observation("AA:BB:CC:DD:EE:01", "192.168.1.21", 1));
    monitor.applyObservation(observation("aa:bb:cc:dd:ee:01", "192.168.1.22", 2));

    expect(callbackMacs.size() == 2, "monitor should report each applied asset update immediately");
    expect(callbackMacs.front() == "aa:bb:cc:dd:ee:01", "monitor should report normalized asset MAC");
    expect(callbackNewFlags.size() == 2 && callbackNewFlags[0] && !callbackNewFlags[1],
        "monitor should distinguish new assets from updates");
}

} // namespace

int main()
{
    emitsOnlyNewAssetEventsAndPreservesSummary();
    reportsAssetUpdatesImmediately();

    if (failures > 0) {
        std::cerr << failures << " asset monitor test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
