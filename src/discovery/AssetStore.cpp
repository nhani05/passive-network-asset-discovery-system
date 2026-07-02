#include "pnad/discovery/AssetStore.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>
#include <vector>

namespace asset_discovery::asset {
namespace {

std::string normalizeMacAddress(std::string macAddress)
{
    std::transform(macAddress.begin(), macAddress.end(), macAddress.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return macAddress;
}

bool containsCaseInsensitive(const std::string& value, const std::string& needle)
{
    auto lowerValue = value;
    auto lowerNeedle = needle;
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    std::transform(lowerNeedle.begin(), lowerNeedle.end(), lowerNeedle.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return lowerValue.find(lowerNeedle) != std::string::npos;
}

std::vector<std::string> splitMacAddress(const std::string& macAddress)
{
    std::vector<std::string> parts;
    std::string part;
    std::istringstream input(macAddress);
    while (std::getline(input, part, ':')) {
        parts.push_back(part);
    }
    return parts;
}

std::optional<unsigned int> parseHexByte(const std::string& value)
{
    if (value.size() != 2) {
        return std::nullopt;
    }
    unsigned int result = 0;
    std::istringstream input(value);
    input >> std::hex >> result;
    if (!input || result > 0xffU) {
        return std::nullopt;
    }
    return result;
}

std::optional<unsigned int> firstMacOctet(const std::string& macAddress)
{
    const auto parts = splitMacAddress(macAddress);
    if (parts.empty()) {
        return std::nullopt;
    }
    return parseHexByte(parts.front());
}

std::optional<std::string> macOui(const std::string& macAddress)
{
    const auto parts = splitMacAddress(macAddress);
    if (parts.size() < 3) {
        return std::nullopt;
    }
    return parts[0] + ":" + parts[1] + ":" + parts[2];
}

std::optional<std::string> embeddedOuiRegistrant(const std::string& oui)
{
    if (oui == "00:1a:11") {
        return "Google";
    }
    if (oui == "00:1b:63") {
        return "Apple";
    }
    if (oui == "00:50:56") {
        return "VMware";
    }
    if (oui == "08:00:27") {
        return "Oracle VirtualBox";
    }
    if (oui == "b8:27:eb") {
        return "Raspberry Pi Foundation";
    }
    if (oui == "dc:a6:32") {
        return "Raspberry Pi Trading";
    }
    return std::nullopt;
}

const std::set<std::string>* metadataValues(
    const parser::StructuredMetadata& metadata,
    const std::string& key)
{
    const auto observed = metadata.observed.find(key);
    if (observed != metadata.observed.end()) {
        return &observed->second.values;
    }
    const auto reference = metadata.reference.find(key);
    if (reference != metadata.reference.end()) {
        return &reference->second.values;
    }
    return nullptr;
}

bool metadataContains(
    const parser::StructuredMetadata& metadata,
    const std::string& key,
    const std::string& needle)
{
    const auto* values = metadataValues(metadata, key);
    if (values == nullptr) {
        return false;
    }
    return std::any_of(values->begin(), values->end(), [&needle](const std::string& value) {
        return containsCaseInsensitive(value, needle);
    });
}

void addMacReferenceMetadata(Asset& asset)
{
    constexpr const char* source = "mac-address";
    const auto firstOctet = firstMacOctet(asset.macAddress);
    if (firstOctet.has_value()) {
        parser::addReferenceMetadata(asset.structuredMetadata,
            "mac.is_multicast",
            ((*firstOctet & 0x01U) != 0U) ? "true" : "false",
            source,
            asset.firstSeen);
        parser::addReferenceMetadata(asset.structuredMetadata,
            "mac.is_locally_administered",
            ((*firstOctet & 0x02U) != 0U) ? "true" : "false",
            source,
            asset.firstSeen);
    }

    const auto oui = macOui(asset.macAddress);
    if (!oui.has_value()) {
        return;
    }
    parser::addReferenceMetadata(asset.structuredMetadata, "mac.oui", *oui, source, asset.firstSeen);

    const auto registrant = embeddedOuiRegistrant(*oui);
    if (registrant.has_value()) {
        parser::addReferenceMetadata(asset.structuredMetadata,
            "mac.oui.registrant",
            *registrant,
            "embedded-minimal-oui",
            asset.firstSeen);
        parser::addReferenceMetadata(asset.structuredMetadata,
            "mac.oui.registry_version",
            "embedded-minimal-oui-v1",
            "embedded-minimal-oui",
            asset.firstSeen);
    }
}

void regenerateDerivedHints(Asset& asset)
{
    asset.structuredMetadata.derivedHints.clear();

    if (metadataContains(asset.structuredMetadata, "mdns.services", "_ipp._tcp.local")
        || metadataContains(asset.structuredMetadata, "mdns.services", "_printer._tcp.local")) {
        parser::addDerivedHint(asset.structuredMetadata,
            {"device_type",
                "printer",
                parser::derivedHintConfidenceHigh,
                "mDNS service advertisement contains a printer service name.",
                {"mdns.services"}});
    }

    if (metadataContains(asset.structuredMetadata, "dhcp.vendor_class_identifier", "MSFT")
        || metadataContains(asset.structuredMetadata, "dhcp.vendor_class_identifier", "Microsoft")) {
        parser::addDerivedHint(asset.structuredMetadata,
            {"os",
                "windows",
                parser::derivedHintConfidenceMedium,
                "DHCP vendor class identifier declares a Microsoft client string.",
                {"dhcp.vendor_class_identifier"}});
    }

    if (metadataValues(asset.structuredMetadata, "tcp.syn_ack_ports") != nullptr) {
        parser::addDerivedHint(asset.structuredMetadata,
            {"role",
                "server_candidate",
                parser::derivedHintConfidenceMedium,
                "The asset sent TCP SYN-ACK packets, which is evidence of accepting inbound TCP connections.",
                {"tcp.syn_ack_ports"}});
    }

    const auto* registrants = metadataValues(asset.structuredMetadata, "mac.oui.registrant");
    if (registrants != nullptr) {
        for (const auto& registrant : *registrants) {
            parser::addDerivedHint(asset.structuredMetadata,
                {"vendor",
                    registrant,
                    parser::derivedHintConfidenceMedium,
                    "OUI registry maps the MAC prefix to this registrant; this is not proof of device vendor.",
                    {"mac.oui.registrant"}});
        }
    }
}

void enrichMetadata(Asset& asset)
{
    addMacReferenceMetadata(asset);
    regenerateDerivedHints(asset);
}

} // namespace

bool timestampLess(const ObservationTimestamp& left, const ObservationTimestamp& right)
{
    if (left.seconds != right.seconds) {
        return left.seconds < right.seconds;
    }
    return left.microseconds < right.microseconds;
}

std::string formatTimestamp(const ObservationTimestamp& timestamp)
{
    std::ostringstream output;
    output << timestamp.seconds << "." << timestamp.microseconds;
    return output.str();
}

void AssetStore::applyObservation(const parser::AssetObservation& observation)
{
    if (observation.macAddress.empty()) {
        return;
    }

    const auto macAddress = normalizeMacAddress(observation.macAddress);
    auto existing = assetsByMac_.find(macAddress);
    if (existing == assetsByMac_.end()) {
        Asset asset;
        asset.macAddress = macAddress;
        asset.firstSeen = observation.timestamp;
        asset.lastSeen = observation.timestamp;
        if (observation.ipAddress.has_value()) {
            asset.ipAddresses.insert(*observation.ipAddress);
        }
        if (observation.hostname.has_value() && !observation.hostname->empty()) {
            asset.hostname = *observation.hostname;
        }
        if (!observation.sourceId.empty()) {
            asset.sources.insert(observation.sourceId);
        }
        asset.metadata = observation.metadata;
        parser::mergeStructuredMetadata(asset.structuredMetadata, observation.structuredMetadata);
        enrichMetadata(asset);
        assetsByMac_.emplace(macAddress, std::move(asset));
        return;
    }

    auto& asset = existing->second;
    if (timestampLess(observation.timestamp, asset.firstSeen)) {
        asset.firstSeen = observation.timestamp;
    }
    if (timestampLess(asset.lastSeen, observation.timestamp)) {
        asset.lastSeen = observation.timestamp;
    }
    if (observation.ipAddress.has_value()) {
        asset.ipAddresses.insert(*observation.ipAddress);
    }
    if (observation.hostname.has_value() && !observation.hostname->empty()) {
        asset.hostname = *observation.hostname;
    }
    if (!observation.sourceId.empty()) {
        asset.sources.insert(observation.sourceId);
    }
    for (const auto& metadata : observation.metadata) {
        asset.metadata[metadata.first] = metadata.second;
    }
    parser::mergeStructuredMetadata(asset.structuredMetadata, observation.structuredMetadata);
    enrichMetadata(asset);
}

std::vector<Asset> AssetStore::assets() const
{
    std::vector<Asset> result;
    result.reserve(assetsByMac_.size());
    for (const auto& item : assetsByMac_) {
        result.push_back(item.second);
    }
    return result;
}

std::size_t AssetStore::size() const
{
    return assetsByMac_.size();
}

} // namespace asset_discovery::asset
