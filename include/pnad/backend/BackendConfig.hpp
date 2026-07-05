#pragma once

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
    std::string listenAddress = "127.0.0.1";
    std::uint16_t port = 8080;
    std::string sqlitePath = "pnad.db";
    std::optional<std::string> databaseUrl;
    BackendCaptureMode captureMode = BackendCaptureMode::Live;
    std::optional<std::string> interfaceName;
    std::optional<std::string> pcapPath;
    std::string packetFilter = "arp or udp port 67 or udp port 68";
    std::string runtimeLogPath = "logs/pnad-runtime.log";
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
