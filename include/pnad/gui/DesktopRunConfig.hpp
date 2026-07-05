#pragma once

#include "pnad/config/AppConfig.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::gui {

enum class DesktopRunMode {
    LiveCapture,
    PcapAnalysis,
};

struct LiveCaptureRequest {
    std::string interfaceName;
};

struct PcapAnalysisRequest {
    std::string pcapPath;
};

struct EnginePreferences {
    std::optional<std::string> preferencesFile;
    std::optional<std::string> presetName;
    std::string captureFilter = "arp or udp port 67 or udp port 68";
    std::string backendPolicy = "auto";
    std::string localDatabasePath;
    std::int64_t duplicateEventSuppressionSeconds = 60;
    std::int64_t eventBufferCapacity = 1024;
    std::int64_t ipChangeDetectionWindowSeconds = 300;
    std::int64_t reappearanceDetectionThresholdSeconds = 15552000;
    std::vector<std::string> localNetworkCidrs;
    std::vector<std::string> ignoredNetworkCidrs;
};

struct ExportPreferences {
    std::string format = "json";
};

struct DesktopRunConfig {
    DesktopRunMode mode = DesktopRunMode::LiveCapture;
    LiveCaptureRequest liveCapture;
    PcapAnalysisRequest pcapAnalysis;
    EnginePreferences engine;
    ExportPreferences exportPreferences;
};

config::ConfigResult buildDesktopAppConfig(
    const DesktopRunConfig& desktopConfig,
    const config::RuntimeEnvironment& runtimeEnvironment,
    const config::BuildConfigOptions& buildOptions = {});

std::optional<std::string> validateDesktopRunConfig(
    const DesktopRunConfig& desktopConfig,
    const config::RuntimeEnvironment& runtimeEnvironment,
    const config::BuildConfigOptions& buildOptions = {});

} // namespace asset_discovery::gui
