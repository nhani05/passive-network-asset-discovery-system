#include "pnad/gui/DesktopRunConfig.hpp"

#include <filesystem>

namespace asset_discovery::gui {
namespace {

std::optional<cli::OutputFormat> parseExportFormat(const std::string& value)
{
    if (value == "table") {
        return cli::OutputFormat::Table;
    }
    if (value == "json") {
        return cli::OutputFormat::Json;
    }
    if (value == "csv") {
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
    if (error == "--capture-backend is only valid with --interface capture") {
        return "Backend policy only applies to Live Capture.";
    }
    if (error == "--filter must not be empty") {
        return "Capture filter must not be empty.";
    }
    if (error == "PostgreSQL or SQLite configuration is required; set DATABASE_URL or SQLITE_DATABASE_PATH/--sqlite") {
        return "Choose a writable local database for desktop storage.";
    }
    if (error == "--config and --profile cannot be combined") {
        return "Use either a preferences file or a preset, not both.";
    }
    if (error == "--profile must contain only letters, digits, underscores, or hyphens") {
        return "Preset name can contain only letters, digits, underscores, or hyphens.";
    }
    return error;
}

config::PatchResult desktopPatch(const DesktopRunConfig& desktopConfig)
{
    config::PatchResult result;

    if (desktopConfig.mode == DesktopRunMode::LiveCapture) {
        result.patch.interfaceName = desktopConfig.liveCapture.interfaceName;
        result.patch.backend = capture::CaptureBackendSelection::Auto;
    } else {
        result.patch.pcapPath = desktopConfig.pcapAnalysis.pcapPath;
        result.patch.backend = capture::CaptureBackendSelection::Auto;
    }

    if (!desktopConfig.engine.captureFilter.empty()) {
        result.patch.packetFilter = desktopConfig.engine.captureFilter;
    } else {
        result.error = "Capture filter must not be empty.";
        return result;
    }

    if (desktopConfig.mode == DesktopRunMode::LiveCapture) {
        const auto backend = capture::parseCaptureBackendSelection(desktopConfig.engine.backendPolicy);
        if (!backend.has_value()) {
            result.error = "Backend policy must be Automatic, pcap, or af-packet.";
            return result;
        }
        result.patch.backend = *backend;
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

    result.patch.eventRateLimitSeconds = desktopConfig.engine.duplicateEventSuppressionSeconds;
    result.patch.eventQueueCapacity = desktopConfig.engine.eventBufferCapacity;
    result.patch.flipFlopWindowSeconds = desktopConfig.engine.ipChangeDetectionWindowSeconds;
    result.patch.reappearanceThresholdSeconds =
        desktopConfig.engine.reappearanceDetectionThresholdSeconds;

    std::vector<asset::Ipv4Network> localNetworks;
    for (const auto& cidr : desktopConfig.engine.localNetworkCidrs) {
        if (cidr.empty()) {
            continue;
        }
        const auto parsed = asset::parseIpv4Network(cidr);
        if (!parsed.has_value()) {
            result.error = "Local networks require valid IPv4 CIDR values.";
            return result;
        }
        localNetworks.push_back(*parsed);
    }
    result.patch.localNetworks = std::move(localNetworks);

    std::vector<asset::Ipv4Network> ignoredNetworks;
    for (const auto& cidr : desktopConfig.engine.ignoredNetworkCidrs) {
        if (cidr.empty()) {
            continue;
        }
        const auto parsed = asset::parseIpv4Network(cidr);
        if (!parsed.has_value()) {
            result.error = "Ignored networks require valid IPv4 CIDR values.";
            return result;
        }
        ignoredNetworks.push_back(*parsed);
    }
    result.patch.ignoredNetworks = std::move(ignoredNetworks);

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
    if (desktopConfig.engine.preferencesFile.has_value()
        && desktopConfig.engine.presetName.has_value()) {
        result.error = "Use either a preferences file or a preset, not both.";
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

    if (desktopConfig.engine.preferencesFile.has_value()) {
        const auto explicitPatch = config::loadConfigFile(*desktopConfig.engine.preferencesFile);
        if (explicitPatch.error.has_value()) {
            result.error = productError(*explicitPatch.error);
            return result;
        }
        config::applyPatch(result.config, explicitPatch.patch);
    } else if (desktopConfig.engine.presetName.has_value()) {
        const auto profilePath = config::resolveProfilePath(
            *desktopConfig.engine.presetName,
            buildOptions.profileDirectory);
        if (!profilePath.has_value()) {
            result.error = "Preset name can contain only letters, digits, underscores, or hyphens.";
            return result;
        }
        const auto profilePatch = config::loadConfigFile(*profilePath);
        if (profilePatch.error.has_value()) {
            result.error = productError(*profilePatch.error);
            return result;
        }
        config::applyPatch(result.config, profilePatch.patch);
    }

    config::applyPatch(result.config, config::patchFromEnvironment(runtimeEnvironment));

    const auto patch = desktopPatch(desktopConfig);
    if (patch.error.has_value()) {
        result.error = *patch.error;
        return result;
    }
    config::applyPatch(result.config, patch.patch);

    if (const auto validationError = config::validateConfig(result.config);
        validationError.has_value()) {
        result.error = productError(*validationError);
        return result;
    }

    return result;
}

} // namespace asset_discovery::gui
