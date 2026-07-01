#pragma once

#include "domain/AssetEventDetector.hpp"
#include "infrastructure/capture/PacketCapture.hpp"

#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::cli {

enum class OutputFormat {
    Table,
    Json,
    Csv,
};

enum class CaptureMode {
    PcapOffline,
    Live,
};

// CLI options after validating relationships between arguments.
struct Options {
    std::optional<std::string> pcapPath;
    std::optional<std::string> interfaceName;
    std::optional<CaptureMode> captureMode;
    std::optional<std::string> packetFilter;
    std::optional<int> eventRateLimitSeconds;
    std::optional<int> eventQueueCapacity;
    std::optional<int> flipFlopWindowSeconds;
    std::optional<int> reappearanceThresholdSeconds;
    std::vector<asset::Ipv4Network> localNetworks;
    std::vector<asset::Ipv4Network> ignoredNetworks;
    capture::CaptureBackendSelection captureBackend = capture::CaptureBackendSelection::Auto;
    OutputFormat outputFormat = OutputFormat::Table;
    bool helpRequested = false;
};

// Parse errors are returned as text so main can choose how to display them.
struct ParseResult {
    Options options;
    std::optional<std::string> error;
};

// Parse argv-style arguments, excluding the executable name.
ParseResult parseArguments(const std::vector<std::string>& args);
std::string usageText(const std::string& executableName);
std::string outputFormatName(OutputFormat format);

} // namespace asset_discovery::cli
