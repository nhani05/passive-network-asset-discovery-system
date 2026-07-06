#include "pnad/gui/DesktopRunConfig.hpp"

#include "pnad/constants/CliConstants.hpp"

#include <filesystem>

namespace asset_discovery::gui {
namespace {

std::optional<cli::OutputFormat> parseExportFormat(const std::string& value)
{
    if (value == constants::cli::OutputTable) {
        return cli::OutputFormat::Table;
    }
    if (value == constants::cli::OutputJson) {
        return cli::OutputFormat::Json;
    }
    if (value == constants::cli::OutputCsv) {
        return cli::OutputFormat::Csv;
    }
    return std::nullopt;
}

std::optional<std::string> nonEmptyValue(const std::string& value)
{
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
}

std::string productError(const std::string& error)
{
    if (error == "provide exactly one input source: --pcap <file> or --interface <name>") {
        return "Choose either Live Capture with a network interface or PCAP Analysis with a PCAP file.";
    }
    if (error == "provide input source: --pcap <file>") {
        return "Choose a PCAP file before starting analysis.";
    }
    if (error == "--filter must not be empty") {
        return "Capture filter must not be empty.";
    }
    if (error == "SQLite configuration is required; set SQLITE_DATABASE_PATH or --sqlite") {
        return "Choose a writable local database for desktop storage.";
    }
    return error;
}

config::PatchResult desktopPatch(const DesktopRunConfig& desktopConfig)
{
    config::PatchResult result;

    if (desktopConfig.mode == DesktopRunMode::LiveCapture) {
        result.patch.interfaceName = desktopConfig.liveCapture.interfaceName;
    } else {
        result.patch.pcapPath = desktopConfig.pcapAnalysis.pcapPath;
    }

    if (!desktopConfig.engine.captureFilter.empty()) {
        result.patch.packetFilter = desktopConfig.engine.captureFilter;
    } else {
        result.error = "Capture filter must not be empty.";
        return result;
    }

    const auto outputFormat = parseExportFormat(desktopConfig.exportPreferences.format);
    if (!outputFormat.has_value()) {
        result.error = "Export format must be table, JSON, or CSV.";
        return result;
    }
    result.patch.outputFormat = *outputFormat;

    result.patch.sqlitePath = nonEmptyValue(desktopConfig.engine.localDatabasePath);
    if (!result.patch.sqlitePath.has_value()) {
        result.error = "Choose a writable local database for desktop storage.";
        return result;
    }

    return result;
}

} // namespace

std::optional<std::string> validateDesktopRunConfig(
    const DesktopRunConfig& desktopConfig,
    const config::RuntimeEnvironment& runtimeEnvironment,
    const config::BuildConfigOptions& buildOptions)
{
    const auto result = buildDesktopAppConfig(desktopConfig, runtimeEnvironment, buildOptions);
    return result.error;
}

config::ConfigResult buildDesktopAppConfig(
    const DesktopRunConfig& desktopConfig,
    const config::RuntimeEnvironment& runtimeEnvironment,
    const config::BuildConfigOptions& buildOptions)
{
    config::ConfigResult result;
    result.config = config::builtInDefaults();

    if (desktopConfig.mode == DesktopRunMode::LiveCapture
        && desktopConfig.liveCapture.interfaceName.empty()) {
        result.error = "Choose a network interface before starting Live Capture.";
        return result;
    }

    if (desktopConfig.mode == DesktopRunMode::PcapAnalysis
        && desktopConfig.pcapAnalysis.pcapPath.empty()) {
        result.error = "Choose a PCAP file before starting analysis.";
        return result;
    }

    if (buildOptions.loadDefaultConfig
        && std::filesystem::exists(buildOptions.defaultConfigPath)) {
        const auto defaultPatch = config::loadConfigFile(buildOptions.defaultConfigPath);
        if (defaultPatch.error.has_value()) {
            result.error = productError(*defaultPatch.error);
            return result;
        }
        config::applyPatch(result.config, defaultPatch.patch);
    }

    config::applyPatch(result.config, config::patchFromEnvironment(runtimeEnvironment));

    const auto patch = desktopPatch(desktopConfig);
    if (patch.error.has_value()) {
        result.error = *patch.error;
        return result;
    }
    config::applyPatch(result.config, patch.patch);

    if (desktopConfig.mode == DesktopRunMode::LiveCapture) {
        if (!result.config.capture.interfaceName.has_value()) {
            result.error = "Choose a network interface before starting Live Capture.";
            return result;
        }
        if (result.config.capture.packetFilter.has_value()
            && result.config.capture.packetFilter->empty()) {
            result.error = "Capture filter must not be empty.";
            return result;
        }
        if (!result.config.database.sqlitePath.has_value()) {
            result.error = "Choose a writable local database for desktop storage.";
            return result;
        }
        return result;
    }

    if (const auto validationError = config::validateConfig(result.config);
        validationError.has_value()) {
        result.error = productError(*validationError);
        return result;
    }

    return result;
}

} // namespace asset_discovery::gui
