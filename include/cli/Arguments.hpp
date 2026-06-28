#pragma once

#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::cli {

enum class OutputFormat {
    Table,
    Json,
};

// Parsed CLI options after validating relationships between arguments.
struct Options {
    std::optional<std::string> pcapPath;
    std::optional<std::string> interfaceName;
    std::optional<int> durationSeconds;
    OutputFormat outputFormat = OutputFormat::Table;
    bool helpRequested = false;
};

// Parser errors are returned as text so main can decide how to render them.
struct ParseResult {
    Options options;
    std::optional<std::string> error;
};

// Parse argv-style arguments without the executable name.
ParseResult parseArguments(const std::vector<std::string>& args);
std::string usageText(const std::string& executableName);
std::string outputFormatName(OutputFormat format);

} // namespace asset_discovery::cli
