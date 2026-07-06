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
    Live,
};

// CLI options after validating relationships between arguments.
struct Options {
    std::optional<std::string> pcapPath;
    std::optional<CaptureMode> captureMode;
    std::optional<std::string> packetFilter;
    std::optional<std::string> sqlitePath;
    OutputFormat outputFormat = OutputFormat::Json;
    bool outputFormatProvided = false;
    bool helpRequested = false;
    bool versionRequested = false;
};

// Parse errors are returned as text so main can choose how to display them.
struct ParseResult {
    Options options;
    std::optional<std::string> error;
};

// Parse argv-style arguments, excluding the executable name.
ParseResult parseArguments(const std::vector<std::string>& args);
std::string usageText(const std::string& executableName);
std::string versionText();
std::string outputFormatName(OutputFormat format);

} // namespace asset_discovery::cli
