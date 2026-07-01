#include "pnad/event/AssetEventDetector.hpp"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using asset_discovery::asset::AssetEventDetector;
using asset_discovery::asset::AssetEventDetectorConfig;
using asset_discovery::asset::AssetEventType;
using asset_discovery::asset::assetEventTypeName;
using asset_discovery::asset::parseIpv4Address;
using asset_discovery::asset::parseIpv4Network;
using asset_discovery::parser::AssetObservation;
using asset_discovery::parser::ObservationTimestamp;
using asset_discovery::parser::sourceIdArp;
using asset_discovery::parser::sourceIdDhcp;

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

void parsesIpv4Networks()
{
    const auto ip = parseIpv4Address("192.168.1.12");
    expect(ip.has_value(), "valid IPv4 address should parse");
    expect(!parseIpv4Address("192.168.1.300").has_value(), "invalid IPv4 octet should fail");

    const auto network = parseIpv4Network("192.168.1.0/24");
    expect(network.has_value(), "valid CIDR should parse");
    if (network.has_value()) {
        expect(network->contains("192.168.1.12"), "network should contain matching IP");
        expect(!network->contains("192.168.2.12"), "network should reject non-matching IP");
    }
    expect(!parseIpv4Network("192.168.1.0").has_value(), "CIDR without prefix should fail");
    expect(!parseIpv4Network("192.168.1.0/33").has_value(), "CIDR prefix over 32 should fail");
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
    }

    events = detector.detectAndRemember(observation("aa:bb:cc:dd:ee:ff", "192.168.1.12", {2, 0}));
    expect(!hasType(events, AssetEventType::NewAsset), "repeated MAC should not emit duplicate new_asset");
}

void detectsMappingChanges()
{
    AssetEventDetector detector;
    detector.detectAndRemember(observation("aa:bb:cc:dd:ee:ff", "192.168.1.12", {1, 0}));
    auto events = detector.detectAndRemember(observation("11:22:33:44:55:66", "192.168.1.12", {2, 0}));
    const auto* macChanged = findType(events, AssetEventType::MacChangedForIp);
    expect(macChanged != nullptr, "IP moving to different MAC should emit mac_changed_for_ip");
    if (macChanged != nullptr) {
        expect(macChanged->oldMacAddress == "aa:bb:cc:dd:ee:ff", "mac change should include old MAC");
        expect(macChanged->newMacAddress == "11:22:33:44:55:66", "mac change should include new MAC");
    }

    events = detector.detectAndRemember(observation("11:22:33:44:55:66", "192.168.1.40", {3, 0}));
    const auto* ipChanged = findType(events, AssetEventType::IpChangedForMac);
    expect(ipChanged != nullptr, "MAC receiving different IP should emit ip_changed_for_mac");
    if (ipChanged != nullptr) {
        expect(ipChanged->oldIpAddress == "192.168.1.12", "IP change should include old IP");
        expect(ipChanged->newIpAddress == "192.168.1.40", "IP change should include new IP");
    }
}

void detectsFlipFlopWithinWindow()
{
    AssetEventDetectorConfig config;
    config.flipFlopWindowSeconds = 10;
    AssetEventDetector detector(config);

    detector.detectAndRemember(observation("aa:bb:cc:dd:ee:ff", "192.168.1.12", {1, 0}));
    detector.detectAndRemember(observation("11:22:33:44:55:66", "192.168.1.12", {2, 0}));
    auto events = detector.detectAndRemember(observation("aa:bb:cc:dd:ee:ff", "192.168.1.12", {5, 0}));
    expect(hasType(events, AssetEventType::IpMacFlipFlop), "return to previous MAC within window should flip-flop");

    AssetEventDetector lateDetector(config);
    lateDetector.detectAndRemember(observation("aa:bb:cc:dd:ee:ff", "192.168.1.12", {1, 0}));
    lateDetector.detectAndRemember(observation("11:22:33:44:55:66", "192.168.1.12", {2, 0}));
    events = lateDetector.detectAndRemember(observation("aa:bb:cc:dd:ee:ff", "192.168.1.12", {20, 0}));
    expect(!hasType(events, AssetEventType::IpMacFlipFlop), "return after window should not flip-flop");
}

void detectsReappearance()
{
    AssetEventDetectorConfig config;
    config.reappearanceThresholdSeconds = 10;
    AssetEventDetector detector(config);

    detector.detectAndRemember(observation("aa:bb:cc:dd:ee:20", "192.168.1.20", {1, 0}));
    auto events = detector.detectAndRemember(observation("aa:bb:cc:dd:ee:20", "192.168.1.20", {5, 0}));
    expect(!hasType(events, AssetEventType::AssetReappeared), "repeat before threshold should not reappear");
    events = detector.detectAndRemember(observation("aa:bb:cc:dd:ee:20", "192.168.1.20", {20, 0}));
    expect(hasType(events, AssetEventType::AssetReappeared), "repeat after threshold should reappear");
}

void detectsLocalNetworkAndIgnoreRules()
{
    AssetEventDetectorConfig config;
    config.localNetworks.push_back(*parseIpv4Network("192.168.1.0/24"));
    AssetEventDetector detector(config);

    auto events = detector.detectAndRemember(observation("aa:bb:cc:dd:ee:01", "10.10.10.5", {1, 0}));
    expect(hasType(events, AssetEventType::NonLocalSourceIp), "non-local IP should emit event");

    events = detector.detectAndRemember(observation("aa:bb:cc:dd:ee:02", "192.168.1.12", {2, 0}));
    expect(!hasType(events, AssetEventType::NonLocalSourceIp), "local IP should not emit non-local event");

    config.ignoredNetworks.push_back(*parseIpv4Network("10.10.10.0/24"));
    AssetEventDetector ignoredDetector(config);
    events = ignoredDetector.detectAndRemember(observation("aa:bb:cc:dd:ee:03", "10.10.10.5", {3, 0}));
    expect(!hasType(events, AssetEventType::NonLocalSourceIp), "ignored IP should not emit non-local event");

    AssetEventDetector noLocalDetector;
    events = noLocalDetector.detectAndRemember(observation("aa:bb:cc:dd:ee:04", "10.10.10.5", {4, 0}));
    expect(!hasType(events, AssetEventType::NonLocalSourceIp), "no local networks should disable non-local event");
}

void detectsMismatch()
{
    AssetEventDetector detector;
    auto mismatch = observation("11:22:33:44:55:66", "192.168.1.12", {1, 0});
    mismatch.metadata["ethernet.source_mac"] = "aa:bb:cc:dd:ee:ff";
    mismatch.metadata["arp.sender_mac"] = "11:22:33:44:55:66";
    auto events = detector.detectAndRemember(mismatch);
    expect(hasType(events, AssetEventType::EthernetArpMacMismatch), "ARP/Ethernet MAC mismatch should emit event");

    auto matching = observation("11:22:33:44:55:66", "192.168.1.13", {2, 0});
    matching.metadata["ethernet.source_mac"] = "11:22:33:44:55:66";
    matching.metadata["arp.sender_mac"] = "11:22:33:44:55:66";
    events = detector.detectAndRemember(matching);
    expect(!hasType(events, AssetEventType::EthernetArpMacMismatch), "matching ARP/Ethernet MAC should not emit event");
}

void detectsHostnameEvents()
{
    AssetEventDetector detector;
    auto first = observation("aa:bb:cc:dd:ee:ff", "192.168.1.12", {1, 0});
    first.sourceId = sourceIdDhcp;
    first.hostname = "laptop-user";
    auto events = detector.detectAndRemember(first);
    expect(hasType(events, AssetEventType::HostnameLearned), "first hostname should emit hostname_learned");

    auto changed = observation("aa:bb:cc:dd:ee:ff", "192.168.1.12", {2, 0});
    changed.sourceId = sourceIdDhcp;
    changed.hostname = "new-name";
    events = detector.detectAndRemember(changed);
    const auto* event = findType(events, AssetEventType::HostnameChanged);
    expect(event != nullptr, "changed hostname should emit hostname_changed");
    if (event != nullptr) {
        expect(event->metadata.at("old_hostname") == "laptop-user", "hostname change should include old hostname");
        expect(event->metadata.at("new_hostname") == "new-name", "hostname change should include new hostname");
    }
}

} // namespace

int main()
{
    parsesIpv4Networks();
    detectsNewAssetOnce();
    detectsMappingChanges();
    detectsFlipFlopWithinWindow();
    detectsReappearance();
    detectsLocalNetworkAndIgnoreRules();
    detectsMismatch();
    detectsHostnameEvents();

    if (failures > 0) {
        std::cerr << failures << " asset event detector test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
