#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace asset_discovery::parser {

struct ObservationTimestamp {
    std::int64_t seconds = 0;
    std::int64_t microseconds = 0;
};

enum class ObservationEventType {
    Seen,
    Update,
    Discovery,
};

inline constexpr const char* sourceIdArp = "arp";
inline constexpr const char* sourceIdDhcp = "dhcp";
inline constexpr const char* sourceIdDns = "dns";
inline constexpr const char* sourceIdLlmnr = "llmnr";
inline constexpr const char* sourceIdMdns = "mdns";
inline constexpr const char* sourceIdNetbios = "netbios";
inline constexpr const char* sourceIdSsdp = "ssdp";
inline constexpr const char* sourceIdTcp = "tcp";

inline constexpr const char* metadataConfidenceObserved = "observed";
inline constexpr const char* metadataConfidenceReference = "reference";
inline constexpr const char* derivedHintConfidenceLow = "low";
inline constexpr const char* derivedHintConfidenceMedium = "medium";
inline constexpr const char* derivedHintConfidenceHigh = "high";

struct MetadataRecord {
    std::string key;
    std::set<std::string> values;
    std::string source;
    std::string confidenceCategory;
    ObservationTimestamp firstSeen;
    ObservationTimestamp lastSeen;
};

struct DerivedHint {
    std::string category;
    std::string value;
    std::string confidence;
    std::string reason;
    std::vector<std::string> evidenceKeys;
};

struct StructuredMetadata {
    std::map<std::string, MetadataRecord> observed;
    std::map<std::string, MetadataRecord> reference;
    std::vector<DerivedHint> derivedHints;
};

struct AssetObservation {
    std::string macAddress;
    std::optional<std::string> ipAddress;
    std::optional<std::string> hostname;
    std::string sourceId = sourceIdArp;
    ObservationEventType eventType = ObservationEventType::Seen;
    float confidence = 1.0F;
    std::map<std::string, std::string> metadata;
    StructuredMetadata structuredMetadata;
    ObservationTimestamp timestamp;
};

std::string observationEventTypeName(ObservationEventType eventType);
void addObservedMetadata(AssetObservation& observation, const std::string& key, const std::string& value);
void addReferenceMetadata(
    StructuredMetadata& metadata,
    const std::string& key,
    const std::string& value,
    const std::string& source,
    ObservationTimestamp timestamp);
void addDerivedHint(StructuredMetadata& metadata, DerivedHint hint);
void mergeStructuredMetadata(StructuredMetadata& target, const StructuredMetadata& source);

} // namespace asset_discovery::parser
