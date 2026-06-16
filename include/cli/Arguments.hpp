#pragma once

#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::cli {

enum class OutputFormat {
    Table,
    Json,
};

struct Options {
    std::optional<std::string> pcapPath;
    std::optional<std::string> interfaceName;
    std::optional<int> durationSeconds;
    OutputFormat outputFormat = OutputFormat::Table;
    bool helpRequested = false;
};

struct ParseResult {
    Options options;
    std::optional<std::string> error;
};

ParseResult parseArguments(const std::vector<std::string>& args);
std::string usageText(const std::string& executableName);
std::string outputFormatName(OutputFormat format);

} // namespace asset_discovery::cli
