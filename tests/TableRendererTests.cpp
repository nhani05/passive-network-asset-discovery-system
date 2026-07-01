#include "pnad/discovery/TableRenderer.hpp"

#include <iostream>
#include <string>

namespace {

using asset_discovery::asset::Asset;
using asset_discovery::output::renderAssetTable;
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
    asset.sources.insert(sourceIdDns);
    return asset;
}

void rendersSingleAsset()
{
    const auto output = renderAssetTable({makeAsset()});

    expect(contains(output, "MAC"), "table should contain MAC header");
    expect(contains(output, "IPs"), "table should contain IPs header");
    expect(contains(output, "First Seen"), "table should contain first seen header");
    expect(contains(output, "Last Seen"), "table should contain last seen header");
    expect(contains(output, "Sources"), "table should contain sources header");
    expect(contains(output, "02:42:ac:11:00:02"), "table should contain asset MAC");
    expect(contains(output, "192.168.1.10"), "table should contain asset IP");
    expect(contains(output, "1699606784.0"), "table should contain first seen timestamp");
    expect(contains(output, "1699606790.10"), "table should contain last seen timestamp");
    expect(contains(output, "arp"), "table should contain ARP source");
    expect(contains(output, "dns"), "table should preserve DNS source");
}

void rendersMultipleIpsDeterministically()
{
    auto asset = makeAsset();
    asset.ipAddresses.insert("192.168.1.2");

    const auto output = renderAssetTable({asset});

    expect(contains(output, "192.168.1.10,192.168.1.2"), "table should join IP set deterministically");
}

void rendersEmptyState()
{
    const auto output = renderAssetTable({});

    expect(contains(output, "No assets discovered."), "empty table should explain no assets were found");
}

} // namespace

int main()
{
    rendersSingleAsset();
    rendersMultipleIpsDeterministically();
    rendersEmptyState();

    if (failures > 0) {
        std::cerr << failures << " table renderer test expectation(s) failed\n";
        return 1;
    }

    return 0;
}
