#pragma once

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
    LiveTimed,
    LiveInfinite,
};

// CLI options after validating relationships between arguments.
struct Options {
    std::optional<std::string> pcapPath;
    std::optional<std::string> interfaceName;
    std::optional<CaptureMode> captureMode;
    std::optional<int> durationSeconds;
    std::optional<int> idleTimeoutSeconds;
    std::optional<int> maxAssets;
    std::optional<std::string> packetFilter;
    std::optional<std::string> databaseUrl;
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
