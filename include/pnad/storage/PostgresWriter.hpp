#pragma once

#include "pnad/event/AssetEvent.hpp"
#include "pnad/discovery/AssetStore.hpp"
#include "pnad/event/EventSink.hpp"

#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::storage {

std::string postgresSchemaSql();
std::string postgresAssetsSql(const std::vector<asset::Asset>& assets);
std::string postgresEventsSql(const std::vector<asset::AssetEvent>& events);
std::optional<std::string> writeAssetsToPostgres(
    const std::optional<std::string>& databaseUrl,
    const std::vector<asset::Asset>& assets);
std::optional<std::string> writeEventsToPostgres(
    const std::optional<std::string>& databaseUrl,
    const std::vector<asset::AssetEvent>& events);

class DatabaseEventSink final : public output::EventSink {
public:
    explicit DatabaseEventSink(std::optional<std::string> databaseUrl = std::nullopt);

    void write(const asset::AssetEvent& event) override;
    void flush() override;

private:
    std::optional<std::string> databaseUrl_;
};

} // namespace asset_discovery::storage
