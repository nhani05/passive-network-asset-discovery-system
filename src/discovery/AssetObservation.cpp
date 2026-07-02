#include "pnad/discovery/AssetObservation.hpp"

#include <algorithm>
#include <utility>

namespace asset_discovery::parser {
namespace {

bool timestampLessOrEqual(const ObservationTimestamp& left, const ObservationTimestamp& right)
{
    if (left.seconds != right.seconds) {
        return left.seconds < right.seconds;
    }
    return left.microseconds <= right.microseconds;
}

MetadataRecord makeMetadataRecord(
    const std::string& key,
    const std::string& value,
    const std::string& source,
    const std::string& confidenceCategory,
    ObservationTimestamp timestamp)
{
    MetadataRecord record;
    record.key = key;
    record.values.insert(value);
    record.source = source;
    record.confidenceCategory = confidenceCategory;
    record.firstSeen = timestamp;
    record.lastSeen = timestamp;
    return record;
}

void mergeMetadataRecord(std::map<std::string, MetadataRecord>& records, MetadataRecord record)
{
    if (record.key.empty() || record.values.empty()) {
        return;
    }

    auto existing = records.find(record.key);
    if (existing == records.end()) {
        records.emplace(record.key, std::move(record));
        return;
    }

    auto& target = existing->second;
    target.values.insert(record.values.begin(), record.values.end());
    if (target.source.empty() && !record.source.empty()) {
        target.source = record.source;
    }
    if (target.confidenceCategory.empty() && !record.confidenceCategory.empty()) {
        target.confidenceCategory = record.confidenceCategory;
    }
    if (timestampLessOrEqual(record.firstSeen, target.firstSeen)) {
        target.firstSeen = record.firstSeen;
    }
    if (timestampLessOrEqual(target.lastSeen, record.lastSeen)) {
        target.lastSeen = record.lastSeen;
    }
}

bool sameHint(const DerivedHint& left, const DerivedHint& right)
{
    return left.category == right.category && left.value == right.value;
}

} // namespace

std::string observationEventTypeName(ObservationEventType eventType)
{
    switch (eventType) {
    case ObservationEventType::Seen:
        return "seen";
    case ObservationEventType::Update:
        return "update";
    case ObservationEventType::Discovery:
        return "discovery";
    }
    return "unknown";
}

void addObservedMetadata(AssetObservation& observation, const std::string& key, const std::string& value)
{
    if (key.empty() || value.empty()) {
        return;
    }
    observation.metadata[key] = value;
    mergeMetadataRecord(observation.structuredMetadata.observed,
        makeMetadataRecord(key, value, observation.sourceId, metadataConfidenceObserved, observation.timestamp));
}

void addReferenceMetadata(
    StructuredMetadata& metadata,
    const std::string& key,
    const std::string& value,
    const std::string& source,
    ObservationTimestamp timestamp)
{
    if (key.empty() || value.empty()) {
        return;
    }
    mergeMetadataRecord(metadata.reference,
        makeMetadataRecord(key, value, source, metadataConfidenceReference, timestamp));
}

void addDerivedHint(StructuredMetadata& metadata, DerivedHint hint)
{
    if (hint.category.empty() || hint.value.empty() || hint.confidence.empty() || hint.reason.empty()
        || hint.evidenceKeys.empty()) {
        return;
    }

    const auto existing = std::find_if(metadata.derivedHints.begin(),
        metadata.derivedHints.end(),
        [&hint](const DerivedHint& candidate) {
            return sameHint(candidate, hint);
        });
    if (existing == metadata.derivedHints.end()) {
        metadata.derivedHints.push_back(std::move(hint));
    }
}

void mergeStructuredMetadata(StructuredMetadata& target, const StructuredMetadata& source)
{
    for (const auto& item : source.observed) {
        mergeMetadataRecord(target.observed, item.second);
    }
    for (const auto& item : source.reference) {
        mergeMetadataRecord(target.reference, item.second);
    }
    for (const auto& hint : source.derivedHints) {
        addDerivedHint(target, hint);
    }
}

} // namespace asset_discovery::parser
