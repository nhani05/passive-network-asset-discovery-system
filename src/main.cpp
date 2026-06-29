#include "domain/AssetStore.hpp"
#include "infrastructure/capture/PacketCapture.hpp"
#include "interface/cli/Arguments.hpp"
#include "infrastructure/output/CsvRenderer.hpp"
#include "infrastructure/output/JsonRenderer.hpp"
#include "infrastructure/output/TableRenderer.hpp"
#include "application/parser/PacketParserFacade.hpp"
#include "infrastructure/storage/PostgresWriter.hpp"

#include <iostream>
#include <cstdlib>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string trimWhitespace(std::string value)
{
    const auto isSpace = [](unsigned char character) {
        return std::isspace(character) != 0;
    };

    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string unquoteDotEnvValue(std::string value)
{
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
    }
    return value;
}

bool setEnvironmentValue(const std::string& key, const std::string& value)
{
    if (key.empty() || value.empty()) {
        return false;
    }

#if defined(_WIN32)
    return _putenv_s(key.c_str(), value.c_str()) == 0;
#else
    return setenv(key.c_str(), value.c_str(), 1) == 0;
#endif
}

void setPostgresEnvironmentFromAlias(const char* aliasName, const char* postgresName)
{
    const char* postgresValue = std::getenv(postgresName);
    if (postgresValue != nullptr && *postgresValue != '\0') {
        return;
    }

    const char* aliasValue = std::getenv(aliasName);
    if (aliasValue == nullptr || *aliasValue == '\0') {
        return;
    }

    setEnvironmentValue(postgresName, aliasValue);
}

void applyPostgresAliasEnvironment()
{
    setPostgresEnvironmentFromAlias("DB_HOST", "PGHOST");
    setPostgresEnvironmentFromAlias("DB_PORT", "PGPORT");
    setPostgresEnvironmentFromAlias("DB_NAME", "PGDATABASE");
    setPostgresEnvironmentFromAlias("DB_USER", "PGUSER");
    setPostgresEnvironmentFromAlias("DB_PASSWORD", "PGPASSWORD");
}

void loadDotEnvFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trimWhitespace(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (line.rfind("export ", 0) == 0) {
            line = trimWhitespace(line.substr(7));
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const auto key = trimWhitespace(line.substr(0, separator));
        auto value = trimWhitespace(line.substr(separator + 1));
        value = unquoteDotEnvValue(value);
        const char* existing = std::getenv(key.c_str());
        if (existing == nullptr || *existing == '\0') {
            setEnvironmentValue(key, value);
        }
    }
}

std::optional<std::string> resolveDatabaseUrl(const asset_discovery::cli::Options& options)
{
    if (options.databaseUrl.has_value()) {
        return options.databaseUrl;
    }

    const char* value = std::getenv("DATABASE_URL");
    if (value != nullptr && *value != '\0') {
        return std::string(value);
    }

    return std::nullopt;
}

bool hasPostgresConnectionEnvironment()
{
    static const char* const names[] = {
        "PGHOST",
        "PGPORT",
        "PGDATABASE",
        "PGUSER",
        "PGPASSWORD",
        "PGSERVICE",
    };

    for (const char* name : names) {
        const char* value = std::getenv(name);
        if (value != nullptr && *value != '\0') {
            return true;
        }
    }
    return false;
}

asset_discovery::parser::ObservationTimestamp toObservationTimestamp(
    const asset_discovery::capture::PacketTimestamp& timestamp)
{
    return {timestamp.seconds, timestamp.microseconds};
}

asset_discovery::asset::AssetStore buildAssetStore(
    const std::vector<asset_discovery::capture::OfflinePacket>& packets)
{
    asset_discovery::asset::AssetStore store;
    for (const auto& packet : packets) {
        if (packet.linkType != asset_discovery::capture::LinkType::Ethernet) {
            continue;
        }

        const auto observations = asset_discovery::parser::parseEthernetObservations(
            packet.bytes,
            toObservationTimestamp(packet.timestamp));
        for (const auto& observation : observations) {
            store.applyObservation(observation);
        }
    }
    return store;
}

int writeDatabaseIfRequested(
    const std::optional<std::string>& databaseUrl,
    const std::vector<asset_discovery::asset::Asset>& assets)
{
    const auto error = asset_discovery::storage::writeAssetsToPostgres(databaseUrl, assets);
    if (error.has_value()) {
        std::cerr << "error: " << *error << "\n";
        return 1;
    }
    return 0;
}

std::string renderAssets(
    const std::vector<asset_discovery::asset::Asset>& assets,
    asset_discovery::cli::OutputFormat format)
{
    switch (format) {
    case asset_discovery::cli::OutputFormat::Table:
        return asset_discovery::output::renderAssetTable(assets);
    case asset_discovery::cli::OutputFormat::Json:
        return asset_discovery::output::renderAssetJson(assets);
    case asset_discovery::cli::OutputFormat::Csv:
        return asset_discovery::output::renderAssetCsv(assets);
    }
    return asset_discovery::output::renderAssetTable(assets);
}

} // namespace

int main(int argc, char* argv[])
{
    loadDotEnvFile(".env");
    applyPostgresAliasEnvironment();

    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const std::string executableName = argc > 0 ? argv[0] : "asset-discovery";
    const auto result = asset_discovery::cli::parseArguments(args);

    if (result.options.helpRequested) {
        std::cout << asset_discovery::cli::usageText(executableName);
        return 0;
    }

    if (result.error.has_value()) {
        std::cerr << "error: " << *result.error << "\n\n"
                  << asset_discovery::cli::usageText(executableName);
        return 2;
    }

    const asset_discovery::capture::PacketCaptureBackend backend;
    if (!backend.pcapAvailable()) {
        std::cerr << "warning: backend " << backend.backendName()
                  << " is not available in this build; packet capture will work after libpcap is installed.\n";
    }

    if (result.options.pcapPath.has_value()) {
        const auto pcapResult = backend.readPcapFile(
            *result.options.pcapPath,
            result.options.packetFilter);
        if (pcapResult.error.has_value()) {
            std::cerr << "error: " << *pcapResult.error << "\n";
            return 1;
        }

        const auto assetStore = buildAssetStore(pcapResult.packets);
        const auto assets = assetStore.assets();
        const auto databaseUrl = resolveDatabaseUrl(result.options);
        if (databaseUrl.has_value() || hasPostgresConnectionEnvironment()) {
            const int dbResult = writeDatabaseIfRequested(databaseUrl, assets);
            if (dbResult != 0) {
                return dbResult;
            }
        }
        std::cout << renderAssets(assets, result.options.outputFormat);
    } else if (result.options.interfaceName.has_value()) {
        asset_discovery::asset::AssetStore store;
        const auto error = backend.captureLive(
            *result.options.interfaceName,
            result.options.durationSeconds,
            result.options.packetFilter,
            [&store](const asset_discovery::capture::OfflinePacket& packet) {
                if (packet.linkType != asset_discovery::capture::LinkType::Ethernet) {
                    return;
                }
                const auto observations = asset_discovery::parser::parseEthernetObservations(
                    packet.bytes,
                    toObservationTimestamp(packet.timestamp));
                for (const auto& observation : observations) {
                    store.applyObservation(observation);
                }
            }
        );

        if (error.has_value()) {
            std::cerr << "error: " << *error << "\n";
            return 1;
        }

        const auto assets = store.assets();
        const auto databaseUrl = resolveDatabaseUrl(result.options);
        if (databaseUrl.has_value() || hasPostgresConnectionEnvironment()) {
            const int dbResult = writeDatabaseIfRequested(databaseUrl, assets);
            if (dbResult != 0) {
                return dbResult;
            }
        }
        std::cout << renderAssets(assets, result.options.outputFormat);
    }

    return 0;
}
