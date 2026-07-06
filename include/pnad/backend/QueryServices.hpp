#pragma once

#include "pnad/backend/BackendConfig.hpp"
#include "pnad/backend/CaptureService.hpp"
#include "pnad/constants/BackendConstants.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::backend {

struct BackendAssetRecord {
    std::string macAddress;
    std::string ipAddressesJson;
    std::string hostname;
    std::string displayName;
    std::string vendor;
    std::string osHint;
    std::string deviceType;
    std::string modelHint;
    std::string firstSeen;
    std::string lastSeen;
    std::string discoverySourcesJson;
};

struct BackendEventRecord {
    std::int64_t id = 0;
    std::string eventTime;
    std::string eventType;
    std::string severity;
    std::string ipAddress;
    std::string macAddress;
    std::string message;
    std::string metadataJson;
};

struct BackendLogRecord {
    std::string message;
};

struct BackendMetricsSnapshot {
    int assetCount = 0;
    int eventCount = 0;
    std::uint64_t captureStarts = 0;
    std::uint64_t captureStops = 0;
};

struct BackendHealthStatus {
    bool healthy = true;
    std::string status = "healthy";
    CaptureServiceStatus capture;
    std::optional<std::string> storageError;
};

class AssetQueryService {
public:
    explicit AssetQueryService(std::string sqlitePath);
    std::vector<BackendAssetRecord> listAssets(int limit = constants::backend::DefaultAssetQueryLimit) const;

private:
    std::string sqlitePath_;
};

class EventQueryService {
public:
    explicit EventQueryService(std::string sqlitePath);
    std::vector<BackendEventRecord> listEvents(int limit = constants::backend::DefaultQueryLimit) const;

private:
    std::string sqlitePath_;
};

class LogQueryService {
public:
    explicit LogQueryService(std::string runtimeLogPath);
    std::vector<BackendLogRecord> recentLogs(int limit = constants::backend::DefaultQueryLimit) const;

private:
    std::string runtimeLogPath_;
};

class MetricsService {
public:
    MetricsService(std::string sqlitePath, const CaptureService& captureService);
    BackendMetricsSnapshot snapshot() const;

private:
    std::string sqlitePath_;
    const CaptureService& captureService_;
};

class HealthService {
public:
    HealthService(std::string sqlitePath, const CaptureService& captureService);
    BackendHealthStatus status() const;

private:
    std::string sqlitePath_;
    const CaptureService& captureService_;
};

} // namespace asset_discovery::backend
