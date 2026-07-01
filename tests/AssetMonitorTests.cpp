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

void detectsBeforeApplyingAndPreservesSummary()
{
    std::vector<AssetEvent> events;
    AssetMonitor monitor({}, [&](const AssetEvent& event) {
        events.push_back(event);
    });

    monitor.applyObservation(observation("aa:bb:cc:dd:ee:ff", "192.168.1.12", 1));
    monitor.applyObservation(observation("aa:bb:cc:dd:ee:ff", "192.168.1.40", 2));

    bool sawIpChange = false;
    for (const auto& event : events) {
        if (event.type == AssetEventType::IpChangedForMac
            && event.oldIpAddress == "192.168.1.12"
            && event.newIpAddress == "192.168.1.40") {
            sawIpChange = true;
        }
    }

    const auto assets = monitor.assets();
    expect(sawIpChange, "monitor should emit event based on previous state");
    expect(assets.size() == 1, "monitor should preserve final asset aggregation");
    if (!assets.empty()) {
        expect(assets.front().ipAddresses.count("192.168.1.12") == 1, "summary should keep old IP");
        expect(assets.front().ipAddresses.count("192.168.1.40") == 1, "summary should keep new IP");
    }
}

} // namespace

int main()
{
    detectsBeforeApplyingAndPreservesSummary();

    if (failures > 0) {
        std::cerr << failures << " asset monitor test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
