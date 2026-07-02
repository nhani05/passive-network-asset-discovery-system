#pragma once

#include "pnad/capture/PacketCapture.hpp"
#include "pnad/cli/Arguments.hpp"
#include "pnad/event/AssetEventDetector.hpp"

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

struct EventPolicySettings {
    std::int64_t rateLimitSeconds = 60;
    std::int64_t queueCapacity = 1024;
    std::int64_t flipFlopWindowSeconds = 300;
    std::int64_t reappearanceThresholdSeconds = 15552000;
};

struct NetworkPolicySettings {
    std::vector<asset::Ipv4Network> localNetworks;
    std::vector<asset::Ipv4Network> ignoredNetworks;
};

struct DatabaseRuntimeSettings {
    std::optional<std::string> url;
    bool configured = false;
};

struct AppConfig {
    CaptureSettings capture;
    OutputSettings output;
    EventPolicySettings events;
    NetworkPolicySettings network;
    DatabaseRuntimeSettings database;
    std::string eventNdjsonPath = "logs/events.ndjson";
};

struct ConfigPatch {
    std::optional<std::string> pcapPath;
    std::optional<std::string> interfaceName;
    std::optional<std::string> packetFilter;
    std::optional<capture::CaptureBackendSelection> backend;
    std::optional<cli::OutputFormat> outputFormat;
    std::optional<std::int64_t> eventRateLimitSeconds;
    std::optional<std::int64_t> eventQueueCapacity;
    std::optional<std::int64_t> flipFlopWindowSeconds;
    std::optional<std::int64_t> reappearanceThresholdSeconds;
    std::optional<std::vector<asset::Ipv4Network>> localNetworks;
    std::optional<std::vector<asset::Ipv4Network>> ignoredNetworks;
    std::optional<std::string> eventNdjsonPath;
    std::optional<std::optional<std::string>> databaseUrl;
    std::optional<bool> databaseConfigured;
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
    std::optional<std::string> databaseUrl;
    bool databaseConfigured = false;
    std::string eventNdjsonPath = "logs/events.ndjson";
};

struct BuildConfigOptions {
    bool loadDefaultConfig = true;
    std::string defaultConfigPath = "configs/default.yaml";
    std::string profileDirectory = "configs";
};

AppConfig builtInDefaults();
PatchResult loadConfigFile(const std::string& path);
std::optional<std::string> resolveProfilePath(
    const std::string& profileName,
    const std::string& profileDirectory = "configs");
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
