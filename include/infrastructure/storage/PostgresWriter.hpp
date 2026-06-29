#pragma once

#include "domain/AssetStore.hpp"

#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::storage {

std::string postgresSchemaSql();
std::string postgresAssetsSql(const std::vector<asset::Asset>& assets);
std::optional<std::string> writeAssetsToPostgres(
    const std::optional<std::string>& databaseUrl,
    const std::vector<asset::Asset>& assets);

} // namespace asset_discovery::storage
