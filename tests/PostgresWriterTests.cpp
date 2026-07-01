#include "infrastructure/storage/PostgresWriter.hpp"

#include <iostream>
#include <string>

namespace {

using asset_discovery::asset::Asset;
using asset_discovery::asset::AssetEvent;
using asset_discovery::asset::AssetEventSeverity;
using asset_discovery::asset::AssetEventType;
using asset_discovery::parser::sourceIdArp;
using asset_discovery::parser::sourceIdDns;
using asset_discovery::storage::postgresAssetsSql;
using asset_discovery::storage::postgresEventsSql;

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

void writesAssetEventsSql()
{
    AssetEvent event;
    event.timestamp = {100, 200};
    event.type = AssetEventType::MacChangedForIp;
    event.severity = AssetEventSeverity::Warning;
    event.ipAddress = "192.168.1.12";
    event.macAddress = "11:22:33:44:55:66";
    event.oldMacAddress = "aa:bb:cc:dd:ee:ff";
    event.newMacAddress = "11:22:33:44:55:66";
    event.protocol = "arp";
    event.interfaceName = "eth0";
    event.message = "IP address is now associated with a different MAC address";
    event.metadata["note"] = "quoted 'value'";

    const auto sql = postgresEventsSql({event});

    expect(contains(sql, "CREATE TABLE IF NOT EXISTS asset_events"), "event SQL should include event schema");
    expect(contains(sql, "INSERT INTO asset_events"), "event SQL should insert events");
    expect(contains(sql, "'mac_changed_for_ip'"), "event SQL should include event type");
    expect(contains(sql, "'warning'"), "event SQL should include severity");
    expect(contains(sql, "'aa:bb:cc:dd:ee:ff'"), "event SQL should include old MAC");
    expect(contains(sql, "'11:22:33:44:55:66'"), "event SQL should include new MAC");
    expect(contains(sql, "'{\"note\":\"quoted ''value''\"}'::jsonb"),
        "event SQL should SQL-escape metadata JSON");
}

} // namespace

int main()
{
    preservesArbitrarySourcesInSql();
    writesAssetEventsSql();

    if (failures > 0) {
        std::cerr << failures << " PostgreSQL writer test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
