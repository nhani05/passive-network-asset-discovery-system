#pragma once

#include "pnad/config/AppConfig.hpp"
#include "pnad/constants/CaptureConstants.hpp"
#include "pnad/constants/CliConstants.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::gui {

enum class DesktopRunMode {
    PcapAnalysis,
};

struct PcapAnalysisRequest {
    std::string pcapPath;
};

struct EnginePreferences {
    std::string captureFilter = constants::capture::DefaultPacketFilter;
    std::string localDatabasePath;
};

struct ExportPreferences {
    std::string format = constants::cli::OutputJson;
};

struct DesktopRunConfig {
    DesktopRunMode mode = DesktopRunMode::PcapAnalysis;
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
