#pragma once

#include "pnad/constants/BackendConstants.hpp"
#include "pnad/constants/CaptureConstants.hpp"
#include "pnad/constants/ConfigConstants.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::backend {

enum class BackendCaptureMode {
    Live,
    PcapOffline,
};

struct BackendConfig {
    std::string listenAddress = constants::backend::DefaultListenAddress;
    std::uint16_t port = constants::backend::DefaultPort;
    std::string sqlitePath = constants::config::DefaultSqlitePath;
    BackendCaptureMode captureMode = BackendCaptureMode::Live;
    std::optional<std::string> interfaceName;
    std::optional<std::string> pcapPath;
    std::string packetFilter = constants::capture::DefaultPacketFilter;
    std::string runtimeLogPath = constants::backend::DefaultRuntimeLogPath;
    bool serve = false;
};

struct BackendParseResult {
    BackendConfig config;
    bool helpRequested = false;
    std::optional<std::string> error;
};

BackendParseResult parseBackendArguments(const std::vector<std::string>& arguments);

std::string backendUsage();
std::string captureModeName(BackendCaptureMode mode);

} // namespace asset_discovery::backend
