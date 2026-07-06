#pragma once

namespace asset_discovery::constants::config {

inline constexpr const char* DefaultConfigPath = "configs/default.yaml";
inline constexpr const char* ProfileDirectory = "configs";
inline constexpr const char* DotEnvPath = ".env";
inline constexpr const char* SqliteDatabasePathEnv = "SQLITE_DATABASE_PATH";
inline constexpr const char* DefaultSqlitePath = "pnad.db";

inline constexpr const char* OutputSection = "output";
inline constexpr const char* CaptureSection = "capture";
inline constexpr const char* DatabaseSection = "database";
inline constexpr const char* EventsSection = "events";
inline constexpr const char* NetworkSection = "network";
inline constexpr const char* OutputFormatKey = "format";
inline constexpr const char* CaptureFilterKey = "filter";
inline constexpr const char* CaptureBackendKey = "backend";

} // namespace asset_discovery::constants::config
