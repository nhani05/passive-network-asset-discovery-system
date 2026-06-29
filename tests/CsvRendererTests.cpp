#include "infrastructure/output/CsvRenderer.hpp"

#include <iostream>
#include <string>

namespace {

using asset_discovery::asset::Asset;
using asset_discovery::output::renderAssetCsv;
using asset_discovery::parser::sourceIdArp;
using asset_discovery::parser::sourceIdDhcp;
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
    asset.hostname = "laptop-user";
    asset.firstSeen = {1699606784, 0};
    asset.lastSeen = {1699606790, 10};
    asset.sources.insert(sourceIdArp);
    asset.sources.insert(sourceIdDhcp);
    asset.sources.insert(sourceIdDns);
    return asset;
}

void rendersHeaderAndAsset()
{
    const auto output = renderAssetCsv({makeAsset()});
    expect(contains(output, "mac_address,ip_addresses,hostname,first_seen,last_seen,discovery_sources\n"),
        "CSV should include deterministic header");
    expect(contains(output, "02:42:ac:11:00:02,192.168.1.10,laptop-user,1699606784.0,1699606790.10,arp;dhcp;dns"),
        "CSV should include asset fields");
}

void escapesQuotedFields()
{
    auto asset = makeAsset();
    asset.hostname = "host,\"one\"";
    const auto output = renderAssetCsv({asset});
    expect(contains(output, "\"host,\"\"one\"\"\""), "CSV should escape quotes and commas");
}

} // namespace

int main()
{
    rendersHeaderAndAsset();
    escapesQuotedFields();

    if (failures > 0) {
        std::cerr << failures << " CSV renderer test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
