#pragma once

#include "pnad/discovery/AssetStore.hpp"

#include <string>
#include <vector>

namespace asset_discovery::output {

std::string renderObservedMetadataJson(const parser::StructuredMetadata& metadata);
std::string renderReferenceMetadataJson(const parser::StructuredMetadata& metadata);
std::string renderDerivedHintsJson(const parser::StructuredMetadata& metadata);
std::string renderAssetJson(const std::vector<asset::Asset>& assets);

} // namespace asset_discovery::output
