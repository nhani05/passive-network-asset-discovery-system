#pragma once

#include "pnad/event/AssetEvent.hpp"
#include "pnad/discovery/AssetStore.hpp"
#include "pnad/event/EventSink.hpp"

#include <sqlite3.h>
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <cstdint>

namespace asset_discovery::storage {

struct AnalysisSessionRecord {
    std::int64_t id = 0;
    std::string mode;
    std::string source;
    std::string startTime;
    std::string endTime;
    std::string status = "Running";
    int assetCount = 0;
    int eventCount = 0;
    std::string errorSummary;
    std::string storageContext;
};

class SQLiteWriter final : public output::EventSink {
public:
    explicit SQLiteWriter(const std::string& dbPath);
    ~SQLiteWriter() override;

    // Disable copy/move
    SQLiteWriter(const SQLiteWriter&) = delete;
    SQLiteWriter& operator=(const SQLiteWriter&) = delete;
    SQLiteWriter(SQLiteWriter&&) = delete;
    SQLiteWriter& operator=(SQLiteWriter&&) = delete;

    // EventSink interface
    void write(const asset::AssetEvent& event) override;
    void flush() override;

    // Asset storage helper
    std::optional<std::string> writeAssets(const std::vector<asset::Asset>& assets);

    // Settings storage helpers
    std::optional<std::string> saveSetting(const std::string& key, const std::string& value);
    std::optional<std::string> getSetting(const std::string& key, std::string& value);

    // Analysis session helpers
    std::optional<std::string> createAnalysisSession(AnalysisSessionRecord& session);
    std::optional<std::string> finishAnalysisSession(
        std::int64_t sessionId,
        const std::string& status,
        int assetCount,
        int eventCount,
        const std::string& errorSummary = "");
    std::vector<AnalysisSessionRecord> loadRecentAnalysisSessions(
        int limit,
        std::optional<std::string>& error);
    std::optional<std::string> countAssets(int& count);
    std::optional<std::string> countEvents(int& count);

private:
    std::optional<std::string> initializeDatabase();

    std::string dbPath_;
    sqlite3* db_ = nullptr;
    std::mutex dbMutex_;
};

} // namespace asset_discovery::storage
