#include "pnad/storage/SQLiteWriter.hpp"
#include <iostream>
#include <string>
#include <cstdio>
#include <sqlite3.h>

namespace {

using asset_discovery::asset::Asset;
using asset_discovery::asset::AssetEvent;
using asset_discovery::asset::AssetEventSeverity;
using asset_discovery::asset::AssetEventType;
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
        bool hasEvents = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (name == "assets") hasAssets = true;
            if (name == "asset_events") hasEvents = true;
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);

        expect(hasAssets, "Database should have assets table");
        expect(hasEvents, "Database should have asset_events table");

        // Write an asset
        Asset asset;
        asset.macAddress = "02:42:ac:11:00:05";
        asset.ipAddresses.insert("192.168.1.50");
        asset.firstSeen = {100, 200};
        asset.lastSeen = {100, 200};
        asset.sources.insert(sourceIdArp);

        auto writeError = writer.writeAssets({asset});
        expect(!writeError.has_value(), "writeAssets should succeed");

        // Write an event
        AssetEvent event;
        event.timestamp = {100, 200};
        event.type = AssetEventType::NewAsset;
        event.severity = AssetEventSeverity::Info;
        event.ipAddress = "192.168.1.50";
        event.macAddress = "02:42:ac:11:00:05";
        event.protocol = "arp";
        event.interfaceName = "eth0";
        event.message = "New asset discovered";
        event.metadata["note"] = "test";

        writer.write(event);

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

        // Verify events serialization
        expect(sqlite3_prepare_v2(db, "SELECT event_type, severity, message FROM asset_events WHERE mac_address='02:42:ac:11:00:05';", -1, &stmt, nullptr) == SQLITE_OK, "Should select event");
        expect(sqlite3_step(stmt) == SQLITE_ROW, "Event should be inserted");
        std::string eventType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string severity = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        sqlite3_finalize(stmt);

        expect(eventType == "new_asset", "Event type should be stringified");
        expect(severity == "info", "Severity should be stringified");
        expect(message == "New asset discovered", "Message should be saved");

        sqlite3_close(db);
    }

    std::remove(dbPath.c_str());
}

void testSQLiteWriterMigrationsAndSettings()
{
    std::string dbPath = "test_sqlite_migrations_temp.db";
    std::remove(dbPath.c_str());

    {
        // 1. First open: should initialize database and migrate to version 3
        SQLiteWriter writer(dbPath);

        sqlite3* db = nullptr;
        expect(sqlite3_open(dbPath.c_str(), &db) == SQLITE_OK, "Should open migrations db");

        // Verify user_version is 3
        sqlite3_stmt* stmt = nullptr;
        expect(sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &stmt, nullptr) == SQLITE_OK, "Prepare version pragma");
        expect(sqlite3_step(stmt) == SQLITE_ROW, "Step version pragma");
        int version = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        expect(version == 3, "Database version should be migrated to 3");

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

    {
        SQLiteWriter writer(dbPath);

        AnalysisSessionRecord session;
        session.mode = "PCAP Analysis";
        session.source = "samples/arp.pcap";
        session.storageContext = dbPath;
        auto error = writer.createAnalysisSession(session);
        expect(!error.has_value(), "createAnalysisSession should succeed");
        expect(session.id > 0, "created session should have an id");

        error = writer.finishAnalysisSession(session.id, "Completed", 3, 4);
        expect(!error.has_value(), "finishAnalysisSession should succeed");

        std::optional<std::string> loadError;
        const auto sessions = writer.loadRecentAnalysisSessions(10, loadError);
        expect(!loadError.has_value(), "loadRecentAnalysisSessions should succeed");
        expect(sessions.size() == 1, "one session should be loaded");
        if (!sessions.empty()) {
            expect(sessions[0].mode == "PCAP Analysis", "session mode should persist");
            expect(sessions[0].source == "samples/arp.pcap", "session source should persist");
            expect(sessions[0].status == "Completed", "session status should update");
            expect(sessions[0].assetCount == 3, "session asset count should update");
            expect(sessions[0].eventCount == 4, "session event count should update");
        }

        AnalysisSessionRecord failed;
        failed.mode = "Live Capture";
        failed.source = "eth0";
        failed.storageContext = dbPath;
        error = writer.createAnalysisSession(failed);
        expect(!error.has_value(), "failed session create should succeed");
        error = writer.finishAnalysisSession(failed.id, "Failed", 0, 0, "Permission missing");
        expect(!error.has_value(), "failed session update should succeed");

        const auto recent = writer.loadRecentAnalysisSessions(1, loadError);
        expect(!loadError.has_value(), "limited recent session load should succeed");
        expect(recent.size() == 1, "limit should restrict recent sessions");
        if (!recent.empty()) {
            expect(recent[0].status == "Failed", "most recent session should be first");
            expect(recent[0].errorSummary == "Permission missing", "error summary should persist");
        }
    }

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
