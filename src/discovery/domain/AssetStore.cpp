#include "pnad/discovery/AssetStore.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
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

struct OuiRegistrant {
    const char* oui;
    const char* registrant;
};

constexpr OuiRegistrant curatedOuiRegistry[] = {
    {"00:03:93", "Apple"},
    {"00:05:02", "Apple"},
    {"00:0a:27", "Apple"},
    {"00:0a:95", "Apple"},
    {"00:14:51", "Apple"},
    {"00:16:cb", "Apple"},
    {"00:17:f2", "Apple"},
    {"00:19:e3", "Apple"},
    {"00:1b:63", "Apple"},
    {"00:1e:52", "Apple"},
    {"00:1f:5b", "Apple"},
    {"00:21:e9", "Apple"},
    {"00:23:12", "Apple"},
    {"00:23:32", "Apple"},
    {"00:25:00", "Apple"},
    {"00:26:08", "Apple"},
    {"04:0c:ce", "Apple"},
    {"3c:15:c2", "Apple"},
    {"40:98:ad", "Apple"},
    {"70:cd:60", "Apple"},
    {"a4:5e:60", "Apple"},
    {"f0:18:98", "Apple"},
    {"00:1a:11", "Google"},
    {"3c:5a:b4", "Google"},
    {"54:60:09", "Google"},
    {"64:16:66", "Google"},
    {"f4:f5:d8", "Google"},
    {"00:50:f2", "Microsoft"},
    {"28:18:78", "Microsoft"},
    {"7c:1e:52", "Microsoft"},
    {"d8:bb:2c", "Microsoft"},
    {"00:15:99", "Samsung"},
    {"00:16:6b", "Samsung"},
    {"00:17:c9", "Samsung"},
    {"00:1d:25", "Samsung"},
    {"00:23:39", "Samsung"},
    {"00:26:37", "Samsung"},
    {"08:08:c2", "Samsung"},
    {"5c:f6:dc", "Samsung"},
    {"a0:21:b7", "Samsung"},
    {"cc:07:e4", "Samsung"},
    {"00:12:17", "Cisco"},
    {"00:14:a9", "Cisco"},
    {"00:1b:54", "Cisco"},
    {"00:1e:13", "Cisco"},
    {"00:21:55", "Cisco"},
    {"00:23:04", "Cisco"},
    {"00:24:14", "Cisco"},
    {"00:40:96", "Cisco"},
    {"00:50:56", "VMware"},
    {"00:05:69", "VMware"},
    {"00:0c:29", "VMware"},
    {"08:00:27", "Oracle VirtualBox"},
    {"52:54:00", "QEMU"},
    {"b8:27:eb", "Raspberry Pi Foundation"},
    {"dc:a6:32", "Raspberry Pi Trading"},
    {"e4:5f:01", "Raspberry Pi Trading"},
    {"24:0a:c4", "Espressif"},
    {"30:ae:a4", "Espressif"},
    {"7c:df:a1", "Espressif"},
    {"a4:cf:12", "Espressif"},
    {"00:1d:0f", "TP-Link"},
    {"14:cc:20", "TP-Link"},
    {"50:c7:bf", "TP-Link"},
    {"64:66:b3", "TP-Link"},
    {"98:da:c4", "TP-Link"},
    {"b0:be:76", "TP-Link"},
    {"d8:0d:17", "TP-Link"},
    {"00:15:6d", "Ubiquiti"},
    {"04:18:d6", "Ubiquiti"},
    {"24:a4:3c", "Ubiquiti"},
    {"44:d9:e7", "Ubiquiti"},
    {"68:d7:9a", "Ubiquiti"},
    {"78:8a:20", "Ubiquiti"},
    {"00:0c:42", "MikroTik"},
    {"18:fd:74", "MikroTik"},
    {"4c:5e:0c", "MikroTik"},
    {"64:d1:54", "MikroTik"},
    {"00:1e:58", "D-Link"},
    {"00:22:b0", "D-Link"},
    {"1c:7e:e5", "D-Link"},
    {"28:10:7b", "D-Link"},
    {"00:13:10", "Cisco Linksys"},
    {"00:18:39", "Cisco Linksys"},
    {"00:25:9c", "Cisco Linksys"},
    {"10:bf:48", "ASUSTek"},
    {"2c:4d:54", "ASUSTek"},
    {"38:2c:4a", "ASUSTek"},
    {"00:1f:c6", "ASUSTek"},
    {"00:1b:21", "Intel"},
    {"00:1c:c0", "Intel"},
    {"00:21:6a", "Intel"},
    {"3c:a9:f4", "Intel"},
    {"f4:06:69", "Intel"},
    {"00:14:22", "Dell"},
    {"00:1c:23", "Dell"},
    {"18:03:73", "Dell"},
    {"b8:ca:3a", "Dell"},
    {"00:1f:29", "Hewlett Packard"},
    {"2c:27:d7", "Hewlett Packard"},
    {"3c:d9:2b", "Hewlett Packard"},
    {"70:10:6f", "Hewlett Packard"},
    {"00:59:07", "Lenovo"},
    {"20:47:47", "Lenovo"},
    {"54:ee:75", "Lenovo"},
    {"a4:db:30", "Lenovo"}
};

std::optional<std::string> curatedOuiRegistrant(const std::string& oui)
{
    const auto match = std::find_if(std::begin(curatedOuiRegistry), std::end(curatedOuiRegistry), [&oui](const auto& item) {
        return oui == item.oui;
    });
    if (match != std::end(curatedOuiRegistry)) {
        return match->registrant;
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

    const auto registrant = curatedOuiRegistrant(*oui);
    if (registrant.has_value()) {
        if (!asset.vendor.has_value()) {
            asset.vendor = *registrant;
        }
        parser::addReferenceMetadata(asset.structuredMetadata,
            "mac.oui.registrant",
            *registrant,
            "curated-oui",
            asset.firstSeen);
        parser::addReferenceMetadata(asset.structuredMetadata,
            "mac.oui.registry_version",
            "curated-oui-v1",
            "curated-oui",
            asset.firstSeen);
    }
}

bool hasValue(const std::optional<std::string>& value)
{
    return value.has_value() && !value->empty();
}

void applySummaryCandidates(Asset& asset, const parser::AssetObservation& observation)
{
    if (hasValue(observation.displayName)) {
        if (observation.sourceId == parser::sourceIdMdns || !hasValue(asset.displayName)
            || (observation.sourceId == parser::sourceIdDhcp && asset.displayName == asset.hostname)) {
            asset.displayName = *observation.displayName;
        }
    }
    if (hasValue(observation.vendor)) {
        asset.vendor = *observation.vendor;
    }
    if (hasValue(observation.deviceType)) {
        if (observation.sourceId == parser::sourceIdMdns || !hasValue(asset.deviceType)) {
            asset.deviceType = *observation.deviceType;
        }
    }
    if (hasValue(observation.modelHint)) {
        if (observation.sourceId == parser::sourceIdMdns || !hasValue(asset.modelHint)) {
            asset.modelHint = *observation.modelHint;
        }
    }
    if (hasValue(observation.osHint)) {
        if (observation.sourceId == parser::sourceIdDhcp || !hasValue(asset.osHint)
            || (observation.sourceId == parser::sourceIdSsdp
                && (asset.osHint == "linux/unix" || asset.osHint == "network-device"))) {
            asset.osHint = *observation.osHint;
        }
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
            if (!hasValue(asset.displayName)) {
                asset.displayName = *observation.hostname;
            }
        }
        applySummaryCandidates(asset, observation);
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
        if (!hasValue(asset.displayName)) {
            asset.displayName = *observation.hostname;
        }
    }
    applySummaryCandidates(asset, observation);
    if (!observation.sourceId.empty()) {
        asset.sources.insert(observation.sourceId);
    }
    for (const auto& metadata : observation.metadata) {
        asset.metadata[metadata.first] = metadata.second;
    }
    parser::mergeStructuredMetadata(asset.structuredMetadata, observation.structuredMetadata);
    enrichMetadata(asset);
}

std::optional<Asset> AssetStore::findByMacAddress(const std::string& macAddress) const
{
    const auto existing = assetsByMac_.find(normalizeMacAddress(macAddress));
    if (existing == assetsByMac_.end()) {
        return std::nullopt;
    }
    return existing->second;
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
