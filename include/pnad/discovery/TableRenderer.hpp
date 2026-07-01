#pragma once

#include "pnad/discovery/AssetStore.hpp"

#include <string>
#include <vector>

namespace asset_discovery::output {

std::string renderAssetTable(const std::vector<asset::Asset>& assets);

} // namespace asset_discovery::output
