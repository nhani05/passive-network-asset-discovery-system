#include "pnad/discovery/JsonRenderer.hpp"

#include <iostream>
#include <string>

namespace {

using asset_discovery::asset::Asset;
using asset_discovery::output::renderAssetJson;
using asset_discovery::parser::sourceIdArp;
using asset_discovery::parser::sourceIdDns;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

bool contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

Asset makeAsset()
{
    Asset asset;
    asset.macAddress = "02:42:ac:11:00:02";
    asset.ipAddresses.insert("192.168.1.10");
    asset.firstSeen = {1699606784, 0};
    asset.lastSeen = {1699606790, 10};
    asset.sources.insert(sourceIdArp);
    return asset;
}

void rendersSingleAsset()
{
    const auto output = renderAssetJson({makeAsset()});

    expect(contains(output, "\"mac_address\": \"02:42:ac:11:00:02\""), "JSON should contain MAC field");
    expect(contains(output, "\"ip_addresses\": [\"192.168.1.10\"]"), "JSON should contain IP array");
    expect(contains(output, "\"first_seen\": \"1699606784.0\""), "JSON should contain first_seen");
    expect(contains(output, "\"last_seen\": \"1699606790.10\""), "JSON should contain last_seen");
    expect(contains(output, "\"discovery_sources\": [\"arp\"]"), "JSON should contain source array");
}

void rendersEmptyArray()
{
    const auto output = renderAssetJson({});

    expect(output == "[]\n", "empty JSON output should be an empty array");
}

void rendersDeterministicArrays()
{
    auto asset = makeAsset();
    asset.ipAddresses.insert("192.168.1.2");
    asset.sources.insert(sourceIdDns);

    const auto output = renderAssetJson({asset});

    expect(contains(output, "\"ip_addresses\": [\"192.168.1.10\", \"192.168.1.2\"]"),
        "JSON should emit IP set in deterministic order");
    expect(contains(output, "\"discovery_sources\": [\"arp\", \"dns\"]"),
        "JSON should preserve arbitrary source ids in deterministic order");
}

void escapesJsonStrings()
{
    auto asset = makeAsset();
    asset.macAddress = "aa:bb:\"cc\\dd";

    const auto output = renderAssetJson({asset});

    expect(contains(output, "\"mac_address\": \"aa:bb:\\\"cc\\\\dd\""),
        "JSON should escape quotes and backslashes");
}

} // namespace

int main()
{
    rendersSingleAsset();
    rendersEmptyArray();
    rendersDeterministicArrays();
    escapesJsonStrings();

    if (failures > 0) {
        std::cerr << failures << " JSON renderer test expectation(s) failed\n";
        return 1;
    }

    return 0;
}
