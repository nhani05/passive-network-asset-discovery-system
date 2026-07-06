#include "pnad/discovery/AssetStore.hpp"

#include <iostream>
#include <string>
#include <utility>

namespace {

using asset_discovery::asset::AssetStore;
using asset_discovery::asset::timestampLess;
using asset_discovery::parser::AssetObservation;
using asset_discovery::parser::ObservationEventType;
using asset_discovery::parser::ObservationTimestamp;
using asset_discovery::parser::observationEventTypeName;
using asset_discovery::parser::sourceIdArp;
using asset_discovery::parser::sourceIdDns;
using asset_discovery::parser::sourceIdDhcp;
using asset_discovery::parser::sourceIdIp;
using asset_discovery::parser::sourceIdMdns;
using asset_discovery::parser::sourceIdSsdp;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

AssetObservation arpObservation(
    std::string macAddress,
    std::string ipAddress,
    ObservationTimestamp timestamp)
{
    AssetObservation observation;
    observation.macAddress = std::move(macAddress);
    observation.ipAddress = std::move(ipAddress);
    observation.sourceId = sourceIdArp;
    observation.timestamp = timestamp;
    return observation;
}

void createsNewAsset()
{
    AssetStore store;
    store.applyObservation(arpObservation("02:42:AC:11:00:02", "192.168.1.10", {10, 100}));

    const auto assets = store.assets();

    expect(assets.size() == 1, "first observation should create one asset");
    if (assets.empty()) {
        return;
    }

    expect(assets.front().macAddress == "02:42:ac:11:00:02", "asset MAC should be normalized");
    expect(assets.front().ipAddresses.count("192.168.1.10") == 1, "asset should contain the observation IP");
    expect(assets.front().firstSeen.seconds == 10, "first_seen seconds should come from the first observation");
    expect(assets.front().lastSeen.seconds == 10, "last_seen seconds should come from the first observation");
    expect(assets.front().sources.count(sourceIdArp) == 1, "asset should contain the ARP source");
}

void repeatedObservationUpdatesLastSeen()
{
    AssetStore store;
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {10, 100}));
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {12, 200}));

    const auto assets = store.assets();

    expect(assets.size() == 1, "same MAC should remain one asset");
    if (assets.empty()) {
        return;
    }

    expect(assets.front().firstSeen.seconds == 10, "first_seen should keep the earliest observation");
    expect(assets.front().firstSeen.microseconds == 100, "first_seen microseconds should keep the earliest observation");
    expect(assets.front().lastSeen.seconds == 12, "last_seen should update to the latest observation");
    expect(assets.front().lastSeen.microseconds == 200, "last_seen microseconds should update to the latest observation");
}

void outOfOrderTimestampsPreserveBounds()
{
    AssetStore store;
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {20, 0}));
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {10, 999999}));
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {25, 1}));

    const auto assets = store.assets();

    expect(assets.size() == 1, "out-of-order observations should merge into one asset");
    if (assets.empty()) {
        return;
    }

    expect(assets.front().firstSeen.seconds == 10, "out-of-order first_seen should use the earliest seconds");
    expect(assets.front().firstSeen.microseconds == 999999, "out-of-order first_seen should keep the earliest microseconds");
    expect(assets.front().lastSeen.seconds == 25, "out-of-order last_seen should use the latest seconds");
    expect(assets.front().lastSeen.microseconds == 1, "out-of-order last_seen should keep the latest microseconds");
}

void mergesDistinctIpAddresses()
{
    AssetStore store;
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {10, 0}));
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.11", {11, 0}));

    const auto assets = store.assets();

    expect(assets.size() == 1, "new IP on the same MAC should not create another asset");
    if (assets.empty()) {
        return;
    }

    expect(assets.front().ipAddresses.size() == 2, "asset should contain both distinct IP addresses");
    expect(assets.front().ipAddresses.count("192.168.1.10") == 1, "asset should keep the original IP");
    expect(assets.front().ipAddresses.count("192.168.1.11") == 1, "asset should add the new IP");
}

void ignoresDuplicateIpAddress()
{
    AssetStore store;
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {10, 0}));
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {11, 0}));

    const auto assets = store.assets();

    expect(assets.size() == 1, "duplicate-IP observations should remain one asset");
    if (assets.empty()) {
        return;
    }

    expect(assets.front().ipAddresses.size() == 1, "duplicate IP should not be stored twice");
}

void comparesTimestampsBySecondsThenMicroseconds()
{
    expect(timestampLess({1, 0}, {1, 1}), "timestamp comparison should use microseconds within the same second");
    expect(timestampLess({1, 999999}, {2, 0}), "timestamp comparison should prioritize seconds first");
    expect(!timestampLess({2, 0}, {1, 999999}), "later seconds should not be less than earlier seconds");
}

void preservesArbitrarySourcesAndMetadata()
{
    AssetStore store;
    AssetObservation observation;
    observation.macAddress = "02:42:ac:11:00:04";
    observation.ipAddress = "192.168.1.44";
    observation.sourceId = sourceIdDns;
    observation.metadata["udp.source_port"] = "5353";
    observation.metadata["udp.destination_port"] = "53";
    observation.timestamp = {30, 0};

    store.applyObservation(observation);
    const auto assets = store.assets();

    expect(assets.size() == 1, "DNS observation should create one asset");
    if (assets.empty()) {
        return;
    }

    expect(assets.front().sources.count(sourceIdDns) == 1, "asset should preserve arbitrary DNS source id");
    const auto metadata = assets.front().metadata.find("udp.destination_port");
    expect(metadata != assets.front().metadata.end(), "asset should preserve observation metadata key");
    if (metadata != assets.front().metadata.end()) {
        expect(metadata->second == "53", "asset should preserve observation metadata value");
    }
}

void observationDefaultsAreStable()
{
    AssetObservation observation;

    expect(observation.sourceId == sourceIdArp, "default source id should preserve ARP behavior");
    expect(observation.eventType == ObservationEventType::Seen, "default event type should be Seen");
    expect(observationEventTypeName(observation.eventType) == "seen", "event type name should render Seen");
    expect(observation.confidence == 1.0F, "default confidence should be 1.0");
    expect(observation.metadata.empty(), "default metadata should be empty");
}

void enrichesVendorFromCuratedOui()
{
    AssetStore store;
    store.applyObservation(arpObservation("b8:27:eb:11:22:33", "192.168.1.10", {10, 0}));
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.11", {10, 1}));
    const auto pi = store.findByMacAddress("b8:27:eb:11:22:33");
    const auto unknown = store.findByMacAddress("02:42:ac:11:00:02");
    expect(pi.has_value() && pi->vendor == "Raspberry Pi Foundation", "known curated OUI should set vendor");
    expect(unknown.has_value() && !unknown->vendor.has_value(), "unknown OUI should leave vendor empty");
}

void explicitVendorOverridesOui()
{
    AssetStore store;
    store.applyObservation(arpObservation("b8:27:eb:11:22:33", "192.168.1.10", {10, 0}));
    AssetObservation mdns;
    mdns.macAddress = "b8:27:eb:11:22:33";
    mdns.vendor = "Acme Device";
    mdns.sourceId = sourceIdMdns;
    mdns.timestamp = {11, 0};
    store.applyObservation(mdns);
    const auto asset = store.findByMacAddress("b8:27:eb:11:22:33");
    expect(asset.has_value() && asset->vendor == "Acme Device", "explicit protocol vendor should override OUI vendor");
}

void appliesSummaryPrecedence()
{
    AssetStore store;
    AssetObservation ttl;
    ttl.macAddress = "00:1a:11:22:33:44";
    ttl.osHint = "linux/unix";
    ttl.sourceId = sourceIdIp;
    ttl.timestamp = {10, 0};
    store.applyObservation(ttl);

    AssetObservation dhcp;
    dhcp.macAddress = "00:1a:11:22:33:44";
    dhcp.hostname = "dhcp-host";
    dhcp.displayName = "dhcp-host";
    dhcp.osHint = "windows";
    dhcp.deviceType = "computer";
    dhcp.sourceId = sourceIdDhcp;
    dhcp.timestamp = {11, 0};
    store.applyObservation(dhcp);

    AssetObservation ssdp;
    ssdp.macAddress = "00:1a:11:22:33:44";
    ssdp.displayName = "SSDP TV";
    ssdp.deviceType = "tv";
    ssdp.modelHint = "Living Room TV";
    ssdp.osHint = "linux";
    ssdp.sourceId = sourceIdSsdp;
    ssdp.timestamp = {12, 0};
    store.applyObservation(ssdp);

    AssetObservation mdns;
    mdns.macAddress = "00:1a:11:22:33:44";
    mdns.displayName = "Nam-iPhone";
    mdns.deviceType = "apple-media";
    mdns.modelHint = "iPhone";
    mdns.sourceId = sourceIdMdns;
    mdns.timestamp = {13, 0};
    store.applyObservation(mdns);

    const auto asset = store.findByMacAddress("00:1a:11:22:33:44");
    expect(asset.has_value(), "summary precedence test should have asset");
    if (!asset.has_value()) {
        return;
    }
    expect(asset->displayName == "Nam-iPhone", "mDNS display name should outrank DHCP and SSDP");
    expect(asset->vendor == "Google", "OUI should fill vendor when no explicit vendor exists");
    expect(asset->osHint == "windows", "DHCP OS hint should outrank TTL and SSDP");
    expect(asset->deviceType == "apple-media", "mDNS device type should outrank SSDP and DHCP");
    expect(asset->modelHint == "iPhone", "mDNS model should outrank SSDP model");
}

} // namespace

int main()
{
    createsNewAsset();
    repeatedObservationUpdatesLastSeen();
    outOfOrderTimestampsPreserveBounds();
    mergesDistinctIpAddresses();
    ignoresDuplicateIpAddress();
    comparesTimestampsBySecondsThenMicroseconds();
    preservesArbitrarySourcesAndMetadata();
    observationDefaultsAreStable();
    enrichesVendorFromCuratedOui();
    explicitVendorOverridesOui();
    appliesSummaryPrecedence();

    if (failures > 0) {
        std::cerr << failures << " asset store test expectation(s) failed\n";
        return 1;
    }

    return 0;
}
