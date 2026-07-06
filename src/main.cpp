#include "pnad/discovery/AssetMonitor.hpp"
#include "pnad/app/LiveCapturePipeline.hpp"
#include "pnad/discovery/AssetStore.hpp"
#include "pnad/capture/PacketCapture.hpp"
#include "pnad/cli/Arguments.hpp"
#include "pnad/config/AppConfig.hpp"
#include "pnad/constants/CaptureConstants.hpp"
#include "pnad/constants/CliConstants.hpp"
#include "pnad/constants/ConfigConstants.hpp"
#include "pnad/discovery/CsvRenderer.hpp"
#include "pnad/event/EventSink.hpp"
#include "pnad/discovery/JsonRenderer.hpp"
#include "pnad/discovery/TableRenderer.hpp"
#include "pnad/packet/PacketParserFacade.hpp"
#include "pnad/storage/SQLiteWriter.hpp"
#include "pnad/error/AppError.hpp"

#include <csignal>
#include <iostream>
#include <cstdlib>
#include <cctype>
#include <fstream>
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
        if (key == asset_discovery::constants::config::SqliteDatabasePathEnv) {
#if defined(_WIN32)
            _putenv_s(key.c_str(), value.c_str());
#else
            setenv(key.c_str(), value.c_str(), 0);
#endif
        }
    }
}

std::optional<std::string> resolveSqlitePath()
{
    const char* value = std::getenv(asset_discovery::constants::config::SqliteDatabasePathEnv);
    if (value != nullptr && *value != '\0') {
        return std::string(value);
    }

    return std::nullopt;
}

asset_discovery::parser::ObservationTimestamp toObservationTimestamp(
    const asset_discovery::capture::PacketTimestamp& timestamp)
{
    return {timestamp.seconds, timestamp.microseconds};
}

asset_discovery::monitor::AssetMonitorConfig makeMonitorConfig(
    const asset_discovery::config::AppConfig& config)
{
    (void)config;
    asset_discovery::monitor::AssetMonitorConfig monitorConfig;
    monitorConfig.interfaceName = asset_discovery::constants::capture::PcapInterfaceName;
    return monitorConfig;
}

struct EventDispatcherResult {
    std::unique_ptr<asset_discovery::output::EventDispatcher> dispatcher;
    std::optional<std::string> error;
};

EventDispatcherResult buildEventDispatcher()
{
    auto dispatcher = std::make_unique<asset_discovery::output::EventDispatcher>();
    dispatcher->addSink(std::make_unique<asset_discovery::output::ConsoleEventSink>(std::cout));
    return {std::move(dispatcher), std::nullopt};
}

std::vector<asset_discovery::asset::Asset> processOfflinePackets(
    const std::vector<asset_discovery::capture::OfflinePacket>& packets,
    const asset_discovery::config::AppConfig& config,
    asset_discovery::output::EventDispatcher* eventDispatcher,
    asset_discovery::storage::SQLiteWriter* databaseWriter)
{
    auto monitorConfig = makeMonitorConfig(config);
    asset_discovery::monitor::AssetMonitor monitor(
        std::move(monitorConfig),
        eventDispatcher != nullptr && !eventDispatcher->empty()
            ? asset_discovery::monitor::AssetMonitor::EventCallback(
                  [eventDispatcher](const asset_discovery::asset::AssetEvent& event) {
                      eventDispatcher->dispatch(event);
                  })
            : asset_discovery::monitor::AssetMonitor::EventCallback{},
        databaseWriter != nullptr
            ? asset_discovery::monitor::AssetMonitor::AssetCallback(
                  [databaseWriter](const asset_discovery::asset::Asset& asset, bool isNew) {
                      if (!isNew) {
                          return;
                      }
                      const auto error = databaseWriter->writeAssets({asset});
                      if (error.has_value()) {
                          throw asset_discovery::DatabaseError(*error);
                      }
                  })
            : asset_discovery::monitor::AssetMonitor::AssetCallback{});

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
    const asset_discovery::config::AppConfig& config,
    const std::vector<asset_discovery::asset::Asset>& assets)
{
    if (config.database.sqlitePath.has_value()) {
        asset_discovery::storage::SQLiteWriter writer(*config.database.sqlitePath);
        const auto error = writer.writeAssets(assets);
        if (error.has_value()) {
            throw asset_discovery::DatabaseError(*error);
        }
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
    const std::vector<asset_discovery::asset::Asset>& assets,
    asset_discovery::storage::SQLiteWriter* databaseWriter)
{
    if (databaseWriter != nullptr) {
        const auto error = databaseWriter->writeAssets(assets);
        if (error.has_value()) {
            throw asset_discovery::DatabaseError(*error);
        }
    } else {
        writeDatabaseIfRequested(config, assets);
    }
    std::cout << renderAssets(assets, config.output.format);
}

} // namespace

int main(int argc, char* argv[])
{
    const std::string executableName = argc > 0
        ? argv[0]
        : asset_discovery::constants::cli::ExecutableName;
    try {
        loadDotEnvFile(asset_discovery::constants::config::DotEnvPath);

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

        const auto sqlitePath = resolveSqlitePath();
        asset_discovery::config::RuntimeEnvironment runtimeEnvironment;
        runtimeEnvironment.sqlitePath = sqlitePath;

        const auto configResult = asset_discovery::config::buildAppConfig(
            result.options,
            runtimeEnvironment);
        if (configResult.error.has_value()) {
            throw asset_discovery::ConfigError(*configResult.error);
        }
        const auto& appConfig = configResult.config;
        auto eventDispatcherResult = buildEventDispatcher();
        if (eventDispatcherResult.error.has_value()) {
            throw asset_discovery::DatabaseError(*eventDispatcherResult.error);
        }
        auto& eventDispatcher = eventDispatcherResult.dispatcher;

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

        std::unique_ptr<asset_discovery::storage::SQLiteWriter> databaseWriter;
        if (appConfig.database.sqlitePath.has_value()) {
            databaseWriter = std::make_unique<asset_discovery::storage::SQLiteWriter>(
                *appConfig.database.sqlitePath);
        }

        const auto assets = processOfflinePackets(
            pcapResult.packets,
            appConfig,
            eventDispatcher && !eventDispatcher->empty() ? eventDispatcher.get() : nullptr,
            databaseWriter.get());
        writeAndRenderAssets(appConfig, assets, databaseWriter.get());
        return 0;
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
