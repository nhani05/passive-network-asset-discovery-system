#include "pnad/backend/QueryServices.hpp"

#include "pnad/constants/BackendConstants.hpp"

#include "pnad/storage/SQLiteWriter.hpp"

#include <algorithm>
#include <fstream>
#include <sqlite3.h>
#include <stdexcept>
#include <utility>

namespace asset_discovery::backend {
namespace {

int normalizedLimit(int limit, int fallback)
{
    return limit > 0 ? limit : fallback;
}

std::string textColumn(sqlite3_stmt* statement, int column)
{
    const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, column));
    return text ? text : "";
}

class SqliteHandle {
public:
    explicit SqliteHandle(const std::string& path)
    {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            std::string message = db_ ? sqlite3_errmsg(db_) : "could not allocate SQLite handle";
            if (db_) {
                sqlite3_close(db_);
                db_ = nullptr;
            }
            throw std::runtime_error(message);
        }
    }

    ~SqliteHandle()
    {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    sqlite3* get() const
    {
        return db_;
    }

private:
    sqlite3* db_ = nullptr;
};

} // namespace

AssetQueryService::AssetQueryService(std::string sqlitePath)
    : sqlitePath_(std::move(sqlitePath))
{
}

std::vector<BackendAssetRecord> AssetQueryService::listAssets(int limit) const
{
    storage::SQLiteWriter initializer(sqlitePath_);
    SqliteHandle db(sqlitePath_);
    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "SELECT mac_address, ip_addresses, hostname, display_name, vendor, os_hint, device_type, model_hint, "
        "first_seen, last_seen, discovery_sources FROM assets ORDER BY mac_address LIMIT ?;";
    if (sqlite3_prepare_v2(db.get(), sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db.get()));
    }

    sqlite3_bind_int(statement, 1, normalizedLimit(limit, constants::backend::DefaultAssetQueryLimit));

    std::vector<BackendAssetRecord> records;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        BackendAssetRecord record;
        record.macAddress = textColumn(statement, 0);
        record.ipAddressesJson = textColumn(statement, 1);
        record.hostname = textColumn(statement, 2);
        record.displayName = textColumn(statement, 3);
        record.vendor = textColumn(statement, 4);
        record.osHint = textColumn(statement, 5);
        record.deviceType = textColumn(statement, 6);
        record.modelHint = textColumn(statement, 7);
        record.firstSeen = textColumn(statement, 8);
        record.lastSeen = textColumn(statement, 9);
        record.discoverySourcesJson = textColumn(statement, 10);
        records.push_back(std::move(record));
    }
    sqlite3_finalize(statement);
    return records;
}

EventQueryService::EventQueryService(std::string sqlitePath)
    : sqlitePath_(std::move(sqlitePath))
{
}

std::vector<BackendEventRecord> EventQueryService::listEvents(int limit) const
{
    storage::SQLiteWriter initializer(sqlitePath_);
    (void)limit;
    return {};
}

LogQueryService::LogQueryService(std::string runtimeLogPath)
    : runtimeLogPath_(std::move(runtimeLogPath))
{
}

std::vector<BackendLogRecord> LogQueryService::recentLogs(int limit) const
{
    std::ifstream input(runtimeLogPath_);
    if (!input) {
        return {};
    }

    std::vector<BackendLogRecord> logs;
    std::string line;
    while (std::getline(input, line)) {
        logs.push_back({line});
    }

    const auto requested = static_cast<std::size_t>(normalizedLimit(limit, 100));
    if (logs.size() > requested) {
        logs.erase(logs.begin(), logs.end() - static_cast<std::ptrdiff_t>(requested));
    }
    return logs;
}

MetricsService::MetricsService(std::string sqlitePath, const CaptureService& captureService)
    : sqlitePath_(std::move(sqlitePath)),
      captureService_(captureService)
{
}

BackendMetricsSnapshot MetricsService::snapshot() const
{
    BackendMetricsSnapshot metrics;
    try {
        storage::SQLiteWriter writer(sqlitePath_);
        (void)writer.countAssets(metrics.assetCount);
        (void)writer.countEvents(metrics.eventCount);
    } catch (const std::exception&) {
    }

    const auto capture = captureService_.status();
    metrics.captureStarts = capture.startCount;
    metrics.captureStops = capture.stopCount;
    return metrics;
}

HealthService::HealthService(std::string sqlitePath, const CaptureService& captureService)
    : sqlitePath_(std::move(sqlitePath)),
      captureService_(captureService)
{
}

BackendHealthStatus HealthService::status() const
{
    BackendHealthStatus health;
    health.capture = captureService_.status();
    try {
        storage::SQLiteWriter writer(sqlitePath_);
        int count = 0;
        if (const auto error = writer.countAssets(count); error.has_value()) {
            health.healthy = false;
            health.status = "degraded";
            health.storageError = *error;
        }
    } catch (const std::exception& error) {
        health.healthy = false;
        health.status = "degraded";
        health.storageError = error.what();
    }
    return health;
}

} // namespace asset_discovery::backend
