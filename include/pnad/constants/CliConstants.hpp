#pragma once

namespace asset_discovery::constants::cli {

inline constexpr const char* ExecutableName = "asset-discovery";
inline constexpr const char* CaptureExecutableName = "asset-capture";

inline constexpr const char* HelpOption = "--help";
inline constexpr const char* ShortHelpOption = "-h";
inline constexpr const char* VersionOption = "--version";
inline constexpr const char* PcapOption = "--pcap";
inline constexpr const char* FilterOption = "--filter";
inline constexpr const char* SqliteOption = "--sqlite";
inline constexpr const char* OutputOption = "--output";

inline constexpr const char* OutputTable = "table";
inline constexpr const char* OutputJson = "json";
inline constexpr const char* OutputCsv = "csv";

} // namespace asset_discovery::constants::cli
