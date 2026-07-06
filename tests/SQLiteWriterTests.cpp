#include "pnad/storage/SQLiteWriter.hpp"
#include <cstdint>
#include <iostream>
#include <string>
#include <cstdio>
#include <sqlite3.h>

namespace {

using asset_discovery::asset::Asset;
using asset_discovery::parser::sourceIdArp;
using asset_discovery::storage::SQLiteWriter;
using asset_discovery::storage::AnalysisSessionRecord;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void testSQLiteWriterWriteAndRead()
{
    std::string dbPath = "test_sqlite_writer_temp.db";
    std::remove(dbPath.c_str());

    {
        SQLiteWriter writer(dbPath);

        // Verify tables are created on initialization
        sqlite3* db = nullptr;
        expect(sqlite3_open(dbPath.c_str(), &db) == SQLITE_OK, "Should open database file");

        sqlite3_stmt* stmt = nullptr;
        expect(sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table';", -1, &stmt, nullptr) == SQLITE_OK, "Should query master table");

        bool hasAssets = false;
        bool hasAssetEvents = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (name == "assets") hasAssets = true;
            if (name == "asset_events") hasAssetEvents = true;
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);

        expect(hasAssets, "Database should have assets table");
        expect(!hasAssetEvents, "Database should not have asset_events table");

        // Write an asset
        Asset asset;
        asset.macAddress = "02:42:ac:11:00:05";
        asset.ipAddresses.insert("192.168.1.50");
        asset.firstSeen = {100, 200};
        asset.lastSeen = {100, 200};
        asset.sources.insert(sourceIdArp);

        auto writeError = writer.writeAssets({asset});
        expect(!writeError.has_value(), "writeAssets should succeed");

        // Read and verify database records
        expect(sqlite3_open(dbPath.c_str(), &db) == SQLITE_OK, "Should re-open database");

        // Verify assets serialization
        expect(sqlite3_prepare_v2(db, "SELECT ip_addresses, discovery_sources FROM assets WHERE mac_address='02:42:ac:11:00:05';", -1, &stmt, nullptr) == SQLITE_OK, "Should select asset");
        expect(sqlite3_step(stmt) == SQLITE_ROW, "Asset should be inserted");
        std::string ipAddrs = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string sources = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        sqlite3_finalize(stmt);

        expect(ipAddrs == "[\"192.168.1.50\"]", "IP addresses should be serialized JSON array");
        expect(sources == "[\"arp\"]", "Sources should be serialized JSON array");

        expect(sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND name='asset_events';", -1, &stmt, nullptr) == SQLITE_OK, "Should query asset_events table absence");
        expect(sqlite3_step(stmt) == SQLITE_DONE, "asset_events table should remain absent");
        sqlite3_finalize(stmt);

        sqlite3_close(db);
    }

    std::remove(dbPath.c_str());
}

void testSQLiteWriterMigrationsAndSettings()
{
    std::string dbPath = "test_sqlite_migrations_temp.db";
    std::remove(dbPath.c_str());

    {
        // 1. First open: should initialize database and migrate to version 4
        SQLiteWriter writer(dbPath);

        sqlite3* db = nullptr;
        expect(sqlite3_open(dbPath.c_str(), &db) == SQLITE_OK, "Should open migrations db");

        // Verify user_version is 4
        sqlite3_stmt* stmt = nullptr;
        expect(sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &stmt, nullptr) == SQLITE_OK, "Prepare version pragma");
        expect(sqlite3_step(stmt) == SQLITE_ROW, "Step version pragma");
        int version = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        expect(version == 4, "Database version should be migrated to 4");

        // Verify table exists
        expect(sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND name='app_settings';", -1, &stmt, nullptr) == SQLITE_OK, "Query app_settings table");
        expect(sqlite3_step(stmt) == SQLITE_ROW, "app_settings table should exist");
        sqlite3_finalize(stmt);

        expect(sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND name='analysis_sessions';", -1, &stmt, nullptr) == SQLITE_OK, "Query analysis_sessions table");
        expect(sqlite3_step(stmt) == SQLITE_ROW, "analysis_sessions table should exist");
        sqlite3_finalize(stmt);

        sqlite3_close(db);

        // 2. Test saving and restoring setting values
        auto saveErr = writer.saveSetting("test_key", "test_value");
        expect(!saveErr.has_value(), "saveSetting should succeed");

        std::string val;
        auto getErr = writer.getSetting("test_key", val);
        expect(!getErr.has_value(), "getSetting should succeed");
        expect(val == "test_value", "Retrieved setting value should match saved");

        // Test update setting value (ON CONFLICT DO UPDATE)
        saveErr = writer.saveSetting("test_key", "new_value");
        expect(!saveErr.has_value(), "saveSetting override should succeed");

        getErr = writer.getSetting("test_key", val);
        expect(!getErr.has_value(), "getSetting override should succeed");
        expect(val == "new_value", "Retrieved setting value should match new value");

        // Test missing setting
        getErr = writer.getSetting("nonexistent_key", val);
        expect(getErr.has_value() && *getErr == "Setting not found", "getSetting nonexistent should fail gracefully");
    }

    std::remove(dbPath.c_str());
}

void testAnalysisSessionPersistence()
{
    std::string dbPath = "test_sqlite_sessions_temp.db";
    std::remove(dbPath.c_str());
    std::int64_t completedId = 0;
    std::int64_t failedId = 0;

    {
        SQLiteWriter writer(dbPath);

        AnalysisSessionRecord session;
        session.mode = "PCAP Analysis";
        session.source = "samples/arp.pcap";
        session.storageContext = dbPath;
        auto error = writer.createAnalysisSession(session);
        expect(!error.has_value(), "createAnalysisSession should succeed");
        expect(session.id > 0, "created session should have an id");
        completedId = session.id;

        error = writer.finishAnalysisSession(session.id, "Completed", 3, 4);
        expect(!error.has_value(), "finishAnalysisSession should succeed");

        AnalysisSessionRecord failed;
        failed.mode = "Live Capture";
        failed.source = "eth0";
        failed.storageContext = dbPath;
        error = writer.createAnalysisSession(failed);
        expect(!error.has_value(), "failed session create should succeed");
        failedId = failed.id;
        error = writer.finishAnalysisSession(failed.id, "Failed", 0, 0, "Permission missing");
        expect(!error.has_value(), "failed session update should succeed");
    }

    sqlite3* db = nullptr;
    expect(sqlite3_open(dbPath.c_str(), &db) == SQLITE_OK, "Should open sessions database");

    sqlite3_stmt* stmt = nullptr;
    const char* completedSql =
        "SELECT mode, source, status, asset_count, event_count FROM analysis_sessions WHERE id = ?;";
    expect(sqlite3_prepare_v2(db, completedSql, -1, &stmt, nullptr) == SQLITE_OK, "Prepare completed session query");
    sqlite3_bind_int64(stmt, 1, completedId);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        expect(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)) == std::string("PCAP Analysis"), "session mode should persist");
        expect(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)) == std::string("samples/arp.pcap"), "session source should persist");
        expect(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)) == std::string("Completed"), "session status should update");
        expect(sqlite3_column_int(stmt, 3) == 3, "session asset count should update");
        expect(sqlite3_column_int(stmt, 4) == 4, "session event count should update");
    } else {
        expect(false, "completed session row should exist");
    }
    sqlite3_finalize(stmt);

    const char* failedSql =
        "SELECT status, error_summary FROM analysis_sessions WHERE id = ?;";
    expect(sqlite3_prepare_v2(db, failedSql, -1, &stmt, nullptr) == SQLITE_OK, "Prepare failed session query");
    sqlite3_bind_int64(stmt, 1, failedId);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        expect(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)) == std::string("Failed"), "failed session status should persist");
        expect(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)) == std::string("Permission missing"), "error summary should persist");
    } else {
        expect(false, "failed session row should exist");
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::remove(dbPath.c_str());
}

void testSQLiteWriterUnwritablePath()
{
    std::string unwritablePath = "/nonexistent_folder_abc_123/pnad.db";
    bool threwException = false;
    try {
        SQLiteWriter writer(unwritablePath);
    } catch (const std::exception& e) {
        threwException = true;
        std::string errMsg = e.what();
        expect(errMsg.find("Failed to initialize SQLite database") != std::string::npos, "Exception message should report initialization failure");
    }
    expect(threwException, "Opening unwritable path should throw exception");
}

} // namespace

int main()
{
    testSQLiteWriterWriteAndRead();
    testSQLiteWriterMigrationsAndSettings();
    testAnalysisSessionPersistence();
    testSQLiteWriterUnwritablePath();

    if (failures > 0) {
        std::cerr << failures << " SQLite writer test expectation(s) failed\n";
        return 1;
    }
    std::cout << "All SQLite writer tests passed successfully!\n";
    return 0;
}
