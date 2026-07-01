#include "pnad/storage/PostgresWriter.hpp"
#include "pnad/error/AppError.hpp"

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <unistd.h>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace asset_discovery::storage {
namespace {

std::string quoteSqlString(const std::string& value)
{
    std::string output = "'";
    for (const char character : value) {
        if (character == '\'') {
            output += "''";
        } else {
            output += character;
        }
    }
    output += "'";
    return output;
}

std::string quoteJsonString(const std::string& value)
{
    std::string output = "\"";
    for (const char character : value) {
        switch (character) {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output += character;
            break;
        }
    }
    output += '"';
    return output;
}

std::string quoteShellString(const std::string& value)
{
    std::string output = "'";
    for (const char character : value) {
        if (character == '\'') {
            output += "'\\''";
        } else {
            output += character;
        }
    }
    output += "'";
    return output;
}

std::string postgresTextArray(const std::set<std::string>& values)
{
    std::ostringstream output;
    output << "ARRAY[";
    bool first = true;
    for (const auto& value : values) {
        if (!first) {
            output << ",";
        }
        output << quoteSqlString(value);
        first = false;
    }
    output << "]::text[]";
    return output.str();
}

std::string postgresOptionalText(const std::optional<std::string>& value)
{
    return value.has_value() ? quoteSqlString(*value) : "NULL";
}

std::string postgresMetadataJson(const std::map<std::string, std::string>& metadata)
{
    std::ostringstream output;
    output << "{";
    bool first = true;
    for (const auto& item : metadata) {
        if (!first) {
            output << ",";
        }
        output << quoteJsonString(item.first) << ":" << quoteJsonString(item.second);
        first = false;
    }
    output << "}";
    return output.str();
}

std::string tempSqlPath()
{
    char path[] = "/tmp/asset-discovery-sql-XXXXXX";
    const int descriptor = mkstemp(path);
    if (descriptor >= 0) {
        close(descriptor);
    }
    return path;
}

bool runPsqlCommandWithRetries(const std::string& command)
{
    constexpr int maxAttempts = 10;
    constexpr auto retryDelay = std::chrono::seconds(2);

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (std::system(command.c_str()) == 0) {
            return true;
        }
        if (attempt < maxAttempts) {
            std::this_thread::sleep_for(retryDelay);
        }
    }
    return false;
}

} // namespace

std::string postgresSchemaSql()
{
    return
        "CREATE TABLE IF NOT EXISTS assets (\n"
        "    mac_address TEXT PRIMARY KEY,\n"
        "    ip_addresses TEXT[] NOT NULL DEFAULT '{}',\n"
        "    hostname TEXT,\n"
        "    first_seen TEXT NOT NULL,\n"
        "    last_seen TEXT NOT NULL,\n"
        "    discovery_sources TEXT[] NOT NULL DEFAULT '{}',\n"
        "    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()\n"
        ");\n"
        "CREATE TABLE IF NOT EXISTS asset_events (\n"
        "    id BIGSERIAL PRIMARY KEY,\n"
        "    event_time TEXT NOT NULL,\n"
        "    event_type TEXT NOT NULL,\n"
        "    severity TEXT NOT NULL,\n"
        "    ip_address TEXT,\n"
        "    mac_address TEXT,\n"
        "    old_ip TEXT,\n"
        "    new_ip TEXT,\n"
        "    old_mac TEXT,\n"
        "    new_mac TEXT,\n"
        "    hostname TEXT,\n"
        "    protocol TEXT,\n"
        "    interface TEXT,\n"
        "    message TEXT,\n"
        "    metadata JSONB NOT NULL DEFAULT '{}'::jsonb,\n"
        "    created_at TIMESTAMPTZ NOT NULL DEFAULT now()\n"
        ");\n";
}

std::string postgresAssetsSql(const std::vector<asset::Asset>& assets)
{
    std::ostringstream sql;
    sql << "\\set ON_ERROR_STOP on\n";
    sql << "SET client_min_messages TO warning;\n";
    sql << postgresSchemaSql();
    for (const auto& asset : assets) {
        sql << "INSERT INTO assets (mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources, updated_at) VALUES ("
            << quoteSqlString(asset.macAddress) << ", "
            << postgresTextArray(asset.ipAddresses) << ", "
            << (asset.hostname.has_value() ? quoteSqlString(*asset.hostname) : "NULL") << ", "
            << quoteSqlString(asset::formatTimestamp(asset.firstSeen)) << ", "
            << quoteSqlString(asset::formatTimestamp(asset.lastSeen)) << ", "
            << postgresTextArray(asset.sources) << ", now()) "
            << "ON CONFLICT (mac_address) DO UPDATE SET "
            << "ip_addresses = EXCLUDED.ip_addresses, "
            << "hostname = COALESCE(EXCLUDED.hostname, assets.hostname), "
            << "first_seen = LEAST(assets.first_seen, EXCLUDED.first_seen), "
            << "last_seen = GREATEST(assets.last_seen, EXCLUDED.last_seen), "
            << "discovery_sources = EXCLUDED.discovery_sources, "
            << "updated_at = now();\n";
    }
    return sql.str();
}

std::string postgresEventsSql(const std::vector<asset::AssetEvent>& events)
{
    std::ostringstream sql;
    sql << "\\set ON_ERROR_STOP on\n";
    sql << "SET client_min_messages TO warning;\n";
    sql << postgresSchemaSql();
    for (const auto& event : events) {
        sql << "INSERT INTO asset_events (event_time, event_type, severity, ip_address, mac_address, "
            << "old_ip, new_ip, old_mac, new_mac, hostname, protocol, interface, message, metadata) VALUES ("
            << quoteSqlString(asset::formatEventTimestamp(event.timestamp)) << ", "
            << quoteSqlString(asset::assetEventTypeName(event.type)) << ", "
            << quoteSqlString(asset::assetEventSeverityName(event.severity)) << ", "
            << postgresOptionalText(event.ipAddress) << ", "
            << postgresOptionalText(event.macAddress) << ", "
            << postgresOptionalText(event.oldIpAddress) << ", "
            << postgresOptionalText(event.newIpAddress) << ", "
            << postgresOptionalText(event.oldMacAddress) << ", "
            << postgresOptionalText(event.newMacAddress) << ", "
            << postgresOptionalText(event.hostname) << ", "
            << (event.protocol.empty() ? "NULL" : quoteSqlString(event.protocol)) << ", "
            << (event.interfaceName.empty() ? "NULL" : quoteSqlString(event.interfaceName)) << ", "
            << (event.message.empty() ? "NULL" : quoteSqlString(event.message)) << ", "
            << quoteSqlString(postgresMetadataJson(event.metadata)) << "::jsonb"
            << ");\n";
    }
    return sql.str();
}

std::optional<std::string> runSqlThroughPsql(
    const std::optional<std::string>& databaseUrl,
    const std::string& sqlText)
{
    const auto path = tempSqlPath();
    {
        std::ofstream sql(path);
        if (!sql) {
            return "could not create a temporary SQL file for PostgreSQL writes";
        }
        sql << sqlText;
    }

    std::string command = "psql -q";
    if (databaseUrl.has_value()) {
        command += " " + quoteShellString(*databaseUrl);
    }
    command += " -f " + quoteShellString(path);
    const bool success = runPsqlCommandWithRetries(command);
    std::remove(path.c_str());
    if (!success) {
        return "PostgreSQL write failed through psql";
    }
    return std::nullopt;
}

std::optional<std::string> writeAssetsToPostgres(
    const std::optional<std::string>& databaseUrl,
    const std::vector<asset::Asset>& assets)
{
    return runSqlThroughPsql(databaseUrl, postgresAssetsSql(assets));
}

std::optional<std::string> writeEventsToPostgres(
    const std::optional<std::string>& databaseUrl,
    const std::vector<asset::AssetEvent>& events)
{
    if (events.empty()) {
        return std::nullopt;
    }
    return runSqlThroughPsql(databaseUrl, postgresEventsSql(events));
}

DatabaseEventSink::DatabaseEventSink(std::optional<std::string> databaseUrl)
    : databaseUrl_(std::move(databaseUrl))
{
}

void DatabaseEventSink::write(const asset::AssetEvent& event)
{
    const auto error = writeEventsToPostgres(databaseUrl_, {event});
    if (error.has_value()) {
        throw DatabaseError(*error);
    }
}

void DatabaseEventSink::flush()
{
}

} // namespace asset_discovery::storage
