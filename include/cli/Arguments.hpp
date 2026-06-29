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

// Tùy chọn CLI sau khi đã kiểm tra quan hệ giữa các tham số.
struct Options {
    std::optional<std::string> pcapPath;
    std::optional<std::string> interfaceName;
    std::optional<int> durationSeconds;
    std::optional<std::string> packetFilter;
    std::optional<std::string> databaseUrl;
    OutputFormat outputFormat = OutputFormat::Table;
    bool helpRequested = false;
};

// Lỗi parse được trả về dạng text để main quyết định cách hiển thị.
struct ParseResult {
    Options options;
    std::optional<std::string> error;
};

// Parse danh sách tham số kiểu argv, không gồm tên executable.
ParseResult parseArguments(const std::vector<std::string>& args);
std::string usageText(const std::string& executableName);
std::string outputFormatName(OutputFormat format);

} // namespace asset_discovery::cli
