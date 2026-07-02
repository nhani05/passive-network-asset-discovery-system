#include "pnad/discovery/AssetMonitor.hpp"
#include "pnad/app/LiveCapturePipeline.hpp"
#include "pnad/discovery/AssetStore.hpp"
#include "pnad/capture/PacketCapture.hpp"
#include "pnad/cli/Arguments.hpp"
#include "pnad/config/AppConfig.hpp"
#include "pnad/discovery/CsvRenderer.hpp"
#include "pnad/event/EventSink.hpp"
#include "pnad/discovery/JsonRenderer.hpp"
#include "pnad/discovery/TableRenderer.hpp"
#include "pnad/packet/PacketParserFacade.hpp"
#include "pnad/storage/PostgresWriter.hpp"
#include "pnad/error/AppError.hpp"

#include <csignal>
#include <iostream>
#include <cstdlib>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

volatile std::sig_atomic_t liveCaptureInterrupted = 0;

void handleLiveCaptureSignal(int)
{
    liveCaptureInterrupted = 1;
}

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

std::optional<std::string> resolveDatabaseUrl()
{
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

bool hasDatabaseConfiguration(const std::optional<std::string>& databaseUrl)
{
    return databaseUrl.has_value() || hasPostgresConnectionEnvironment();
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

asset_discovery::monitor::AssetMonitorConfig makeMonitorConfig(
    const asset_discovery::config::AppConfig& config)
{
    asset_discovery::monitor::AssetMonitorConfig monitorConfig;
    if (config.capture.interfaceName.has_value()) {
        monitorConfig.detector.interfaceName = *config.capture.interfaceName;
    }
    monitorConfig.eventRateLimitSeconds = config.events.rateLimitSeconds;
    monitorConfig.detector.flipFlopWindowSeconds = config.events.flipFlopWindowSeconds;
    monitorConfig.detector.reappearanceThresholdSeconds = config.events.reappearanceThresholdSeconds;
    monitorConfig.detector.localNetworks = config.network.localNetworks;
    monitorConfig.detector.ignoredNetworks = config.network.ignoredNetworks;
    return monitorConfig;
}

struct EventDispatcherResult {
    std::unique_ptr<asset_discovery::output::EventDispatcher> dispatcher;
    std::optional<std::string> error;
};

std::string defaultEventNdjsonPath()
{
    const char* value = std::getenv("ASSET_DISCOVERY_EVENTS_JSON");
    if (value != nullptr && *value != '\0') {
        return value;
    }
    return "logs/events.ndjson";
}

std::optional<std::string> ensureEventLogParentDirectory(const std::string& path)
{
    const std::filesystem::path eventPath(path);
    const auto parent = eventPath.parent_path();
    if (parent.empty() || std::filesystem::exists(parent)) {
        return std::nullopt;
    }

    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
        return "could not create event log directory '" + parent.string() + "': " + error.message();
    }
    return std::nullopt;
}

EventDispatcherResult buildEventDispatcher(
    const std::optional<std::string>& databaseUrl,
    const std::string& ndjsonPath)
{
    auto dispatcher = std::make_unique<asset_discovery::output::EventDispatcher>();

    dispatcher->addSink(std::make_unique<asset_discovery::output::ConsoleEventSink>(std::cout));

    if (const auto error = ensureEventLogParentDirectory(ndjsonPath); error.has_value()) {
        return {nullptr, *error};
    }
    auto ndjsonSink = std::make_unique<asset_discovery::output::NdjsonEventSink>(ndjsonPath);
    if (!ndjsonSink->ok()) {
        return {nullptr, ndjsonSink->error()};
    }
    dispatcher->addSink(std::move(ndjsonSink));

    if (asset_discovery::output::SyslogEventSink::supported()) {
        auto sink = std::make_unique<asset_discovery::output::SyslogEventSink>();
        if (sink->ok()) {
            dispatcher->addSink(std::move(sink));
        }
    }

    dispatcher->addSink(std::make_unique<asset_discovery::storage::DatabaseEventSink>(databaseUrl));

    return {std::move(dispatcher), std::nullopt};
}

std::vector<asset_discovery::asset::Asset> processOfflinePackets(
    const std::vector<asset_discovery::capture::OfflinePacket>& packets,
    const asset_discovery::config::AppConfig& config,
    asset_discovery::output::EventDispatcher* eventDispatcher)
{
    auto monitorConfig = makeMonitorConfig(config);
    asset_discovery::monitor::AssetMonitor monitor(
        std::move(monitorConfig),
        eventDispatcher != nullptr && !eventDispatcher->empty()
            ? asset_discovery::monitor::AssetMonitor::EventCallback(
                  [eventDispatcher](const asset_discovery::asset::AssetEvent& event) {
                      eventDispatcher->dispatch(event);
                  })
            : asset_discovery::monitor::AssetMonitor::EventCallback{});

    for (const auto& packet : packets) {
        if (packet.linkType != asset_discovery::capture::LinkType::Ethernet) {
            continue;
        }

        const auto observations = asset_discovery::parser::parseEthernetObservations(
            packet.bytes,
            toObservationTimestamp(packet.timestamp));
        for (const auto& observation : observations) {
            monitor.applyObservation(observation);
        }
    }

    if (eventDispatcher != nullptr) {
        eventDispatcher->flush();
    }
    return monitor.assets();
}

void writeDatabaseIfRequested(
    const std::optional<std::string>& databaseUrl,
    const std::vector<asset_discovery::asset::Asset>& assets)
{
    const auto error = asset_discovery::storage::writeAssetsToPostgres(databaseUrl, assets);
    if (error.has_value()) {
        throw asset_discovery::DatabaseError(*error);
    }
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

void writeAndRenderAssets(
    const asset_discovery::config::AppConfig& config,
    const std::vector<asset_discovery::asset::Asset>& assets)
{
    writeDatabaseIfRequested(config.database.url, assets);
    std::cout << renderAssets(assets, config.output.format);
}

} // namespace

int main(int argc, char* argv[])
{
    const std::string executableName = argc > 0 ? argv[0] : "asset-discovery";
    try {
        loadDotEnvFile(".env");
        applyPostgresAliasEnvironment();

        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        const auto result = asset_discovery::cli::parseArguments(args);

        if (result.options.helpRequested) {
            std::cout << asset_discovery::cli::usageText(executableName);
            return 0;
        }

        if (result.options.versionRequested) {
            std::cout << asset_discovery::cli::versionText();
            return 0;
        }

        if (result.error.has_value()) {
            throw asset_discovery::ConfigError(*result.error);
        }

        const auto databaseUrl = resolveDatabaseUrl();
        asset_discovery::config::RuntimeEnvironment runtimeEnvironment;
        runtimeEnvironment.databaseUrl = databaseUrl;
        runtimeEnvironment.databaseConfigured = hasDatabaseConfiguration(databaseUrl);
        runtimeEnvironment.eventNdjsonPath = defaultEventNdjsonPath();

        const auto configResult = asset_discovery::config::buildAppConfig(
            result.options,
            runtimeEnvironment);
        if (configResult.error.has_value()) {
            throw asset_discovery::ConfigError(*configResult.error);
        }
        const auto& appConfig = configResult.config;
        const auto captureMode = asset_discovery::config::captureMode(appConfig);

        auto eventDispatcherResult = buildEventDispatcher(
            appConfig.database.url,
            appConfig.eventNdjsonPath);
        if (eventDispatcherResult.error.has_value()) {
            throw asset_discovery::DatabaseError(*eventDispatcherResult.error);
        }
        auto& eventDispatcher = eventDispatcherResult.dispatcher;

        if (captureMode == asset_discovery::cli::CaptureMode::PcapOffline) {
            const asset_discovery::capture::PacketCaptureBackend backend;
            if (!backend.pcapAvailable()) {
                std::cerr << "warning: backend " << backend.backendName()
                          << " is not available in this build; packet capture will work after libpcap is installed.\n";
            }

            const auto pcapResult = backend.readPcapFile(
                *appConfig.capture.pcapPath,
                appConfig.capture.packetFilter);
            if (pcapResult.error.has_value()) {
                throw asset_discovery::PcapError(*pcapResult.error);
            }

            const auto assets = processOfflinePackets(
                pcapResult.packets,
                appConfig,
                eventDispatcher && !eventDispatcher->empty() ? eventDispatcher.get() : nullptr);
            writeAndRenderAssets(appConfig, assets);
            return 0;
        } else if (captureMode == asset_discovery::cli::CaptureMode::Live) {
            auto backendResult = asset_discovery::capture::createCaptureBackend(appConfig.capture.backend);
            if (backendResult.error.has_value() || !backendResult.backend) {
                throw asset_discovery::CaptureError(backendResult.error.value_or("capture backend could not be created"));
            }

            asset_discovery::capture::LiveCaptureOptions liveOptions;
            liveCaptureInterrupted = 0;
            liveOptions.stopRequested = []() {
                return liveCaptureInterrupted != 0;
            };

            using SignalHandler = void (*)(int);
            const SignalHandler previousInterruptHandler = std::signal(SIGINT, handleLiveCaptureSignal);
            const SignalHandler previousTerminateHandler = std::signal(SIGTERM, handleLiveCaptureSignal);

            asset_discovery::capture::CaptureConfig captureConfig;
            captureConfig.interfaceName = *appConfig.capture.interfaceName;
            captureConfig.liveOptions = liveOptions;
            captureConfig.packetFilter = appConfig.capture.packetFilter;
            captureConfig.requestedBackend = appConfig.capture.backend;

            asset_discovery::live::LivePipelineOptions livePipelineOptions;
            livePipelineOptions.monitorConfig = makeMonitorConfig(appConfig);
            livePipelineOptions.eventQueueCapacity = static_cast<std::size_t>(appConfig.events.queueCapacity);
            if (eventDispatcher && !eventDispatcher->empty()) {
                livePipelineOptions.eventCallback = [&eventDispatcher](const asset_discovery::asset::AssetEvent& event) {
                    eventDispatcher->dispatch(event);
                };
                livePipelineOptions.eventFlushCallback = [&eventDispatcher]() {
                    eventDispatcher->flush();
                };
            }

            const auto liveResult = asset_discovery::live::runLiveCapturePipeline(
                *backendResult.backend,
                std::move(captureConfig),
                std::move(backendResult.initialStats),
                std::move(livePipelineOptions)
            );

            if (previousInterruptHandler != SIG_ERR) {
                std::signal(SIGINT, previousInterruptHandler);
            }
            if (previousTerminateHandler != SIG_ERR) {
                std::signal(SIGTERM, previousTerminateHandler);
            }

            std::cerr << asset_discovery::live::formatLivePipelineMetrics(liveResult.stats);

            if (liveResult.error.has_value()) {
                const auto& errStr = *liveResult.error;
                if (errStr.find("PostgreSQL") != std::string::npos || errStr.find("database") != std::string::npos || errStr.find("psql") != std::string::npos || errStr.find("Database") != std::string::npos) {
                    throw asset_discovery::DatabaseError(errStr);
                } else {
                    throw asset_discovery::CaptureError(errStr);
                }
            }

            const auto assets = liveResult.assets;
            writeAndRenderAssets(appConfig, assets);
            return 0;
        }
    }
    catch (const asset_discovery::ConfigError& e) {
        std::cerr << "[CONFIG ERROR] " << e.what() << "\n\n"
                  << asset_discovery::cli::usageText(executableName);
        return 2;
    }
    catch (const asset_discovery::PcapError& e) {
        std::cerr << "[PCAP ERROR] " << e.what() << std::endl;
        return 3;
    }
    catch (const asset_discovery::CaptureError& e) {
        std::cerr << "[CAPTURE ERROR] " << e.what() << std::endl;
        return 3;
    }
    catch (const asset_discovery::DatabaseError& e) {
        std::cerr << "[DATABASE ERROR] " << e.what() << std::endl;
        return 4;
    }
    catch (const std::exception& e) {
        std::cerr << "[FATAL ERROR] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
