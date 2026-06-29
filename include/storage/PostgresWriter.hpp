#pragma once

#include "asset/AssetStore.hpp"

#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::storage {

std::string postgresSchemaSql();
std::optional<std::string> writeAssetsToPostgres(
    const std::optional<std::string>& databaseUrl,
    const std::vector<asset::Asset>& assets);

} // namespace asset_discovery::storage
