#include "pnad/storage/SQLiteWriter.hpp"
#include "pnad/error/AppError.hpp"

#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace asset_discovery::storage {
namespace {

std::string toJsonArray(const std::set<std::string>& values)
{
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (const auto& val : values) {
        if (!first) {
            out << ",";
        }
        out << "\"" << val << "\"";
        first = false;
    }
    out << "]";
    return out.str();
}

std::string utcTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm utc = {};
#if defined(_WIN32)
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

} // namespace

SQLiteWriter::SQLiteWriter(const std::string& dbPath)
    : dbPath_(dbPath)
{
    auto error = initializeDatabase();
    if (error.has_value()) {
        throw DatabaseError("Failed to initialize SQLite database: " + *error);
    }
}

SQLiteWriter::~SQLiteWriter()
{
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

std::optional<std::string> SQLiteWriter::initializeDatabase()
{
    const std::filesystem::path path(dbPath_);
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) {
            return "Cannot create database directory '" + parent.string() + "': " + error.message();
        }
    }

    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = db_ ? sqlite3_errmsg(db_) : "unknown error";
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return "Cannot open database: " + err;
    }

    char* zErrMsg = nullptr;
    rc = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::string err = zErrMsg ? zErrMsg : "unknown error";
        sqlite3_free(zErrMsg);
        return "Failed to set WAL mode: " + err;
    }

    // Get current user version
    int version = 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA user_version;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            version = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Migration loop
    if (version < 1) {
        const char* schemaSql =
            "CREATE TABLE IF NOT EXISTS assets (\n"
            "    mac_address TEXT PRIMARY KEY,\n"
            "    ip_addresses TEXT NOT NULL DEFAULT '[]',\n"
            "    hostname TEXT,\n"
            "    display_name TEXT,\n"
            "    vendor TEXT,\n"
            "    os_hint TEXT,\n"
            "    device_type TEXT,\n"
            "    model_hint TEXT,\n"
            "    first_seen TEXT NOT NULL,\n"
            "    last_seen TEXT NOT NULL,\n"
            "    discovery_sources TEXT NOT NULL DEFAULT '[]',\n"
            "    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP\n"
            ");\n";
        rc = sqlite3_exec(db_, schemaSql, nullptr, nullptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            std::string err = zErrMsg ? zErrMsg : "unknown error";
            sqlite3_free(zErrMsg);
            return "Failed to initialize baseline schema: " + err;
        }

        rc = sqlite3_exec(db_, "PRAGMA user_version = 1;", nullptr, nullptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            std::string err = zErrMsg ? zErrMsg : "unknown error";
            sqlite3_free(zErrMsg);
            return "Failed to set schema version to 1: " + err;
        }
        version = 1;
    }

    if (version < 2) {
        // Migration to version 2: add app_settings table
        const char* settingsSql =
            "CREATE TABLE IF NOT EXISTS app_settings (\n"
            "    key TEXT PRIMARY KEY,\n"
            "    value TEXT NOT NULL\n"
            ");\n";
        rc = sqlite3_exec(db_, settingsSql, nullptr, nullptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            std::string err = zErrMsg ? zErrMsg : "unknown error";
            sqlite3_free(zErrMsg);
            return "Failed to create app_settings table (v2 migration): " + err;
        }

        rc = sqlite3_exec(db_, "PRAGMA user_version = 2;", nullptr, nullptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            std::string err = zErrMsg ? zErrMsg : "unknown error";
            sqlite3_free(zErrMsg);
            return "Failed to set schema version to 2: " + err;
        }
        version = 2;
    }

    if (version < 3) {
        const char* sessionsSql =
            "CREATE TABLE IF NOT EXISTS analysis_sessions (\n"
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
            "    mode TEXT NOT NULL,\n"
            "    source TEXT NOT NULL,\n"
            "    start_time TEXT NOT NULL,\n"
            "    end_time TEXT,\n"
            "    status TEXT NOT NULL,\n"
            "    asset_count INTEGER NOT NULL DEFAULT 0,\n"
            "    event_count INTEGER NOT NULL DEFAULT 0,\n"
            "    error_summary TEXT,\n"
            "    storage_context TEXT,\n"
            "    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
            "    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP\n"
            ");\n";
        rc = sqlite3_exec(db_, sessionsSql, nullptr, nullptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            std::string err = zErrMsg ? zErrMsg : "unknown error";
            sqlite3_free(zErrMsg);
            return "Failed to create analysis_sessions table (v3 migration): " + err;
        }

        rc = sqlite3_exec(db_, "PRAGMA user_version = 3;", nullptr, nullptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            std::string err = zErrMsg ? zErrMsg : "unknown error";
            sqlite3_free(zErrMsg);
            return "Failed to set schema version to 3: " + err;
        }
        version = 3;
    }

    if (version < 4) {
        const char* dropEventLogSql = "DROP TABLE IF EXISTS asset_events;\n";
        rc = sqlite3_exec(db_, dropEventLogSql, nullptr, nullptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            std::string err = zErrMsg ? zErrMsg : "unknown error";
            sqlite3_free(zErrMsg);
            return "Failed to remove asset_events table (v4 migration): " + err;
        }

        rc = sqlite3_exec(db_, "PRAGMA user_version = 4;", nullptr, nullptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            std::string err = zErrMsg ? zErrMsg : "unknown error";
            sqlite3_free(zErrMsg);
            return "Failed to set schema version to 4: " + err;
        }
        version = 4;
    }

    if (version < 5) {
        const char* migrateAssetsSql =
            "BEGIN TRANSACTION;\n"
            "CREATE TABLE IF NOT EXISTS assets_new (\n"
            "    mac_address TEXT PRIMARY KEY,\n"
            "    ip_addresses TEXT NOT NULL DEFAULT '[]',\n"
            "    hostname TEXT,\n"
            "    display_name TEXT,\n"
            "    vendor TEXT,\n"
            "    os_hint TEXT,\n"
            "    device_type TEXT,\n"
            "    model_hint TEXT,\n"
            "    first_seen TEXT NOT NULL,\n"
            "    last_seen TEXT NOT NULL,\n"
            "    discovery_sources TEXT NOT NULL DEFAULT '[]',\n"
            "    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP\n"
            ");\n"
            "INSERT OR REPLACE INTO assets_new (\n"
            "    mac_address, ip_addresses, hostname, display_name, vendor, os_hint, device_type, model_hint,\n"
            "    first_seen, last_seen, discovery_sources, updated_at\n"
            ")\n"
            "SELECT mac_address, ip_addresses, hostname, hostname, NULL, NULL, NULL, NULL,\n"
            "       first_seen, last_seen, discovery_sources, updated_at\n"
            "FROM assets;\n"
            "DROP TABLE assets;\n"
            "ALTER TABLE assets_new RENAME TO assets;\n"
            "PRAGMA user_version = 5;\n"
            "COMMIT;\n";
        rc = sqlite3_exec(db_, migrateAssetsSql, nullptr, nullptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            std::string err = zErrMsg ? zErrMsg : "unknown error";
            sqlite3_free(zErrMsg);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            return "Failed to migrate assets schema to version 5: " + err;
        }
        version = 5;
    }

    return std::nullopt;
}

std::optional<std::string> SQLiteWriter::createAnalysisSession(AnalysisSessionRecord& session)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) {
        return "Database not open";
    }
    if (session.startTime.empty()) {
        session.startTime = utcTimestamp();
    }
    if (session.status.empty()) {
        session.status = "Running";
    }

    const char* sql =
        "INSERT INTO analysis_sessions (mode, source, start_time, status, asset_count, event_count, error_summary, storage_context, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::string("Failed to prepare create session statement: ") + sqlite3_errmsg(db_);
    }

    sqlite3_bind_text(stmt, 1, session.mode.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, session.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, session.startTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, session.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, session.assetCount);
    sqlite3_bind_int(stmt, 6, session.eventCount);
    if (session.errorSummary.empty()) {
        sqlite3_bind_null(stmt, 7);
    } else {
        sqlite3_bind_text(stmt, 7, session.errorSummary.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (session.storageContext.empty()) {
        sqlite3_bind_null(stmt, 8);
    } else {
        sqlite3_bind_text(stmt, 8, session.storageContext.c_str(), -1, SQLITE_TRANSIENT);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return std::string("Failed to create analysis session: ") + sqlite3_errmsg(db_);
    }

    session.id = sqlite3_last_insert_rowid(db_);
    return std::nullopt;
}

std::optional<std::string> SQLiteWriter::finishAnalysisSession(
    std::int64_t sessionId,
    const std::string& status,
    int assetCount,
    int eventCount,
    const std::string& errorSummary)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) {
        return "Database not open";
    }

    const std::string endTime = utcTimestamp();
    const char* sql =
        "UPDATE analysis_sessions SET end_time = ?, status = ?, asset_count = ?, event_count = ?, "
        "error_summary = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::string("Failed to prepare finish session statement: ") + sqlite3_errmsg(db_);
    }

    sqlite3_bind_text(stmt, 1, endTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, assetCount);
    sqlite3_bind_int(stmt, 4, eventCount);
    if (errorSummary.empty()) {
        sqlite3_bind_null(stmt, 5);
    } else {
        sqlite3_bind_text(stmt, 5, errorSummary.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int64(stmt, 6, sessionId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return std::string("Failed to finish analysis session: ") + sqlite3_errmsg(db_);
    }
    return std::nullopt;
}

std::optional<std::string> SQLiteWriter::countAssets(int& count)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) {
        return "Database not open";
    }
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM assets;", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::string("Failed to prepare asset count query: ") + sqlite3_errmsg(db_);
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    sqlite3_finalize(stmt);
    return std::string("Failed to count assets: ") + sqlite3_errmsg(db_);
}

std::optional<std::string> SQLiteWriter::countEvents(int& count)
{
    count = 0;
    return std::nullopt;
}

std::optional<std::string> SQLiteWriter::clearApplicationData()
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) {
        return "Database not open";
    }

    const char* sql =
        "BEGIN TRANSACTION;"
        "DELETE FROM assets;"
        "DELETE FROM analysis_sessions;"
        "DELETE FROM app_settings;"
        "DELETE FROM sqlite_sequence WHERE name = 'analysis_sessions';"
        "COMMIT;";

    char* zErrMsg = nullptr;
    const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        const std::string err = zErrMsg ? zErrMsg : "unknown error";
        sqlite3_free(zErrMsg);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return "Failed to clear application data: " + err;
    }

    return std::nullopt;
}

std::optional<std::string> SQLiteWriter::saveSetting(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) {
        return "Database not open";
    }

    const char* sql = "INSERT INTO app_settings (key, value) VALUES (?, ?)\n"
                      "ON CONFLICT(key) DO UPDATE SET value=excluded.value;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::string("Failed to prepare save statement: ") + sqlite3_errmsg(db_);
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return std::string("Failed to save setting: ") + sqlite3_errmsg(db_);
    }

    return std::nullopt;
}

std::optional<std::string> SQLiteWriter::getSetting(const std::string& key, std::string& value)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) {
        return "Database not open";
    }

    const char* sql = "SELECT value FROM app_settings WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::string("Failed to prepare get statement: ") + sqlite3_errmsg(db_);
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* valText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (valText) {
            value = valText;
        } else {
            value = "";
        }
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
        return "Setting not found";
    }
    return std::string("Failed to fetch setting: ") + sqlite3_errmsg(db_);
}

std::optional<std::string> SQLiteWriter::writeAssets(const std::vector<asset::Asset>& assets)
{
    if (assets.empty()) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(dbMutex_);

    char* zErrMsg = nullptr;
    int rc = sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::string err = zErrMsg ? zErrMsg : "unknown error";
        sqlite3_free(zErrMsg);
        return "Failed to begin transaction: " + err;
    }

    const char* sql =
        "INSERT INTO assets (mac_address, ip_addresses, hostname, display_name, vendor, os_hint, device_type, "
        "model_hint, first_seen, last_seen, discovery_sources, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP) "
        "ON CONFLICT(mac_address) DO UPDATE SET "
        "ip_addresses = excluded.ip_addresses, "
        "hostname = COALESCE(excluded.hostname, assets.hostname), "
        "display_name = COALESCE(excluded.display_name, assets.display_name), "
        "vendor = COALESCE(excluded.vendor, assets.vendor), "
        "os_hint = COALESCE(excluded.os_hint, assets.os_hint), "
        "device_type = COALESCE(excluded.device_type, assets.device_type), "
        "model_hint = COALESCE(excluded.model_hint, assets.model_hint), "
        "first_seen = CASE WHEN excluded.first_seen < assets.first_seen THEN excluded.first_seen ELSE assets.first_seen END, "
        "last_seen = CASE WHEN excluded.last_seen > assets.last_seen THEN excluded.last_seen ELSE assets.last_seen END, "
        "discovery_sources = excluded.discovery_sources, "
        "updated_at = CURRENT_TIMESTAMP;";

    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return "Failed to prepare insert asset statement: " + err;
    }

    for (const auto& asset : assets) {
        sqlite3_bind_text(stmt, 1, asset.macAddress.c_str(), -1, SQLITE_TRANSIENT);

        std::string ipJson = toJsonArray(asset.ipAddresses);
        sqlite3_bind_text(stmt, 2, ipJson.c_str(), -1, SQLITE_TRANSIENT);

        if (asset.hostname.has_value()) {
            sqlite3_bind_text(stmt, 3, asset.hostname->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 3);
        }

        if (asset.displayName.has_value()) {
            sqlite3_bind_text(stmt, 4, asset.displayName->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 4);
        }

        if (asset.vendor.has_value()) {
            sqlite3_bind_text(stmt, 5, asset.vendor->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 5);
        }

        if (asset.osHint.has_value()) {
            sqlite3_bind_text(stmt, 6, asset.osHint->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 6);
        }

        if (asset.deviceType.has_value()) {
            sqlite3_bind_text(stmt, 7, asset.deviceType->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 7);
        }

        if (asset.modelHint.has_value()) {
            sqlite3_bind_text(stmt, 8, asset.modelHint->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 8);
        }

        std::string firstSeenStr = asset::formatTimestamp(asset.firstSeen);
        sqlite3_bind_text(stmt, 9, firstSeenStr.c_str(), -1, SQLITE_TRANSIENT);

        std::string lastSeenStr = asset::formatTimestamp(asset.lastSeen);
        sqlite3_bind_text(stmt, 10, lastSeenStr.c_str(), -1, SQLITE_TRANSIENT);

        std::string sourcesJson = toJsonArray(asset.sources);
        sqlite3_bind_text(stmt, 11, sourcesJson.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::string err = sqlite3_errmsg(db_);
            sqlite3_finalize(stmt);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            return "Failed to insert asset " + asset.macAddress + ": " + err;
        }

        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);

    rc = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::string err = zErrMsg ? zErrMsg : "unknown error";
        sqlite3_free(zErrMsg);
        return "Failed to commit transaction: " + err;
    }

    return std::nullopt;
}

} // namespace asset_discovery::storage
