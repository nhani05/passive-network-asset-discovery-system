#pragma once

#include "domain/AssetStore.hpp"

#include <string>
#include <vector>

namespace asset_discovery::output {

std::string renderAssetCsv(const std::vector<asset::Asset>& assets);

} // namespace asset_discovery::output
