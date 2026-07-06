#include "pnad/discovery/JsonRenderer.hpp"

#include <iomanip>
#include <cstddef>
#include <set>
#include <sstream>
#include <string>

namespace asset_discovery::output {
namespace {

std::string escapeJsonString(const std::string& value)
{
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20) {
                output << "\\u"
                       << std::hex << std::nouppercase << std::setw(4) << std::setfill('0')
                       << static_cast<int>(character)
                       << std::dec << std::setfill(' ');
            } else {
                output << static_cast<char>(character);
            }
            break;
        }
    }
    return output.str();
}

void appendStringArray(std::ostringstream& output, const std::set<std::string>& values)
{
    output << "[";
    bool first = true;
    for (const auto& value : values) {
        if (!first) {
            output << ", ";
        }
        output << "\"" << escapeJsonString(value) << "\"";
        first = false;
    }
    output << "]";
}

void appendStringVector(std::ostringstream& output, const std::vector<std::string>& values)
{
    output << "[";
    bool first = true;
    for (const auto& value : values) {
        if (!first) {
            output << ", ";
        }
        output << "\"" << escapeJsonString(value) << "\"";
        first = false;
    }
    output << "]";
}

void appendSourceArray(std::ostringstream& output, const std::set<std::string>& sources)
{
    output << "[";
    bool first = true;
    for (const auto& source : sources) {
        if (!first) {
            output << ", ";
        }
        output << "\"" << escapeJsonString(source) << "\"";
        first = false;
    }
    output << "]";
}

} // namespace

std::string renderObservedMetadataJson(const parser::StructuredMetadata& metadata)
{
    std::ostringstream output;
    (void)metadata;
    output << "{}";
    return output.str();
}

std::string renderReferenceMetadataJson(const parser::StructuredMetadata& metadata)
{
    std::ostringstream output;
    (void)metadata;
    output << "{}";
    return output.str();
}

std::string renderDerivedHintsJson(const parser::StructuredMetadata& metadata)
{
    std::ostringstream output;
    (void)metadata;
    output << "[]";
    return output.str();
}

std::string renderAssetJson(const std::vector<asset::Asset>& assets)
{
    std::ostringstream output;
    output << "[";
    if (!assets.empty()) {
        output << "\n";
    }

    for (std::size_t index = 0; index < assets.size(); ++index) {
        const auto& asset = assets[index];
        output << "  {\n";
        output << "    \"mac_address\": \"" << escapeJsonString(asset.macAddress) << "\",\n";
        output << "    \"ip_addresses\": ";
        appendStringArray(output, asset.ipAddresses);
        output << ",\n";
        if (asset.hostname.has_value()) {
            output << "    \"hostname\": \"" << escapeJsonString(*asset.hostname) << "\",\n";
        }
        output << "    \"display_name\": \"" << escapeJsonString(asset.displayName.value_or("")) << "\",\n";
        output << "    \"vendor\": \"" << escapeJsonString(asset.vendor.value_or("")) << "\",\n";
        output << "    \"os_hint\": \"" << escapeJsonString(asset.osHint.value_or("")) << "\",\n";
        output << "    \"device_type\": \"" << escapeJsonString(asset.deviceType.value_or("")) << "\",\n";
        output << "    \"model_hint\": \"" << escapeJsonString(asset.modelHint.value_or("")) << "\",\n";
        output << "    \"first_seen\": \"" << escapeJsonString(asset::formatTimestamp(asset.firstSeen)) << "\",\n";
        output << "    \"last_seen\": \"" << escapeJsonString(asset::formatTimestamp(asset.lastSeen)) << "\",\n";
        output << "    \"discovery_sources\": ";
        appendSourceArray(output, asset.sources);
        output << "\n";
        output << "  }";
        if (index + 1 < assets.size()) {
            output << ",";
        }
        output << "\n";
    }

    output << "]\n";
    return output.str();
}

} // namespace asset_discovery::output
