#pragma once

#include "pnad/capture/PacketCapture.hpp"
#include "pnad/cli/Arguments.hpp"
#include "pnad/constants/ConfigConstants.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::config {

struct CaptureSettings {
    std::optional<std::string> pcapPath;
    std::optional<std::string> interfaceName;
    std::optional<std::string> packetFilter;
    capture::CaptureBackendSelection backend = capture::CaptureBackendSelection::Auto;
};

struct OutputSettings {
    cli::OutputFormat format = cli::OutputFormat::Json;
};

struct DatabaseRuntimeSettings {
    std::optional<std::string> sqlitePath;
};

struct AppConfig {
    CaptureSettings capture;
    OutputSettings output;
    DatabaseRuntimeSettings database;
};

struct ConfigPatch {
    std::optional<std::string> pcapPath;
    std::optional<std::string> interfaceName;
    std::optional<std::string> packetFilter;
    std::optional<capture::CaptureBackendSelection> backend;
    std::optional<cli::OutputFormat> outputFormat;
    std::optional<std::optional<std::string>> sqlitePath;
};

struct ConfigResult {
    AppConfig config;
    std::optional<std::string> error;
};

struct PatchResult {
    ConfigPatch patch;
    std::optional<std::string> error;
};

struct RuntimeEnvironment {
    std::optional<std::string> sqlitePath;
};

struct BuildConfigOptions {
    bool loadDefaultConfig = true;
    std::string defaultConfigPath = constants::config::DefaultConfigPath;
};

AppConfig builtInDefaults();
PatchResult loadConfigFile(const std::string& path);
ConfigPatch patchFromCliOptions(const cli::Options& options);
ConfigPatch patchFromEnvironment(const RuntimeEnvironment& environment);
void applyPatch(AppConfig& config, const ConfigPatch& patch);
std::optional<std::string> validateConfig(const AppConfig& config);
ConfigResult buildAppConfig(
    const cli::Options& options,
    const RuntimeEnvironment& environment,
    const BuildConfigOptions& buildOptions = {});
cli::CaptureMode captureMode(const AppConfig& config);

} // namespace asset_discovery::config
