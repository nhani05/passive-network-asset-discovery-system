#include "storage/PostgresWriter.hpp"

#include "parser/AssetObservation.hpp"

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <unistd.h>
#include <fstream>
#include <set>
#include <sstream>
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

std::string postgresSourceArray(const std::set<parser::ObservationSource>& sources)
{
    std::set<std::string> values;
    for (const auto& source : sources) {
        values.insert(parser::observationSourceName(source));
    }
    return postgresTextArray(values);
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
        ");\n";
}

std::optional<std::string> writeAssetsToPostgres(
    const std::optional<std::string>& databaseUrl,
    const std::vector<asset::Asset>& assets)
{
    const auto path = tempSqlPath();
    {
        std::ofstream sql(path);
        if (!sql) {
            return "could not create a temporary SQL file for PostgreSQL writes";
        }
        sql << "\\set ON_ERROR_STOP on\n";
        sql << postgresSchemaSql();
        for (const auto& asset : assets) {
            sql << "INSERT INTO assets (mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources, updated_at) VALUES ("
                << quoteSqlString(asset.macAddress) << ", "
                << postgresTextArray(asset.ipAddresses) << ", "
                << (asset.hostname.has_value() ? quoteSqlString(*asset.hostname) : "NULL") << ", "
                << quoteSqlString(asset::formatTimestamp(asset.firstSeen)) << ", "
                << quoteSqlString(asset::formatTimestamp(asset.lastSeen)) << ", "
                << postgresSourceArray(asset.sources) << ", now()) "
                << "ON CONFLICT (mac_address) DO UPDATE SET "
                << "ip_addresses = EXCLUDED.ip_addresses, "
                << "hostname = COALESCE(EXCLUDED.hostname, assets.hostname), "
                << "first_seen = LEAST(assets.first_seen, EXCLUDED.first_seen), "
                << "last_seen = GREATEST(assets.last_seen, EXCLUDED.last_seen), "
                << "discovery_sources = EXCLUDED.discovery_sources, "
                << "updated_at = now();\n";
        }
    }

    std::string command = "psql";
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

} // namespace asset_discovery::storage
