#include "infrastructure/storage/PostgresWriter.hpp"

#include <iostream>
#include <string>

namespace {

using asset_discovery::asset::Asset;
using asset_discovery::parser::sourceIdArp;
using asset_discovery::parser::sourceIdDns;
using asset_discovery::storage::postgresAssetsSql;

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

void preservesArbitrarySourcesInSql()
{
    Asset asset;
    asset.macAddress = "02:42:ac:11:00:05";
    asset.ipAddresses.insert("192.168.1.50");
    asset.firstSeen = {100, 200};
    asset.lastSeen = {100, 200};
    asset.sources.insert(sourceIdArp);
    asset.sources.insert(sourceIdDns);

    const auto sql = postgresAssetsSql({asset});

    expect(contains(sql, "CREATE TABLE IF NOT EXISTS assets"), "SQL helper should include schema");
    expect(contains(sql, "SET client_min_messages TO warning"), "SQL helper should suppress expected schema notices");
    expect(contains(sql, "INSERT INTO assets"), "SQL helper should include asset insert");
    expect(contains(sql, "ARRAY['arp','dns']::text[]"), "SQL helper should preserve arbitrary source ids");
}

} // namespace

int main()
{
    preservesArbitrarySourcesInSql();

    if (failures > 0) {
        std::cerr << failures << " PostgreSQL writer test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
