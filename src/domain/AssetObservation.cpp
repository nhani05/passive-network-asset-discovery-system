#include "domain/AssetObservation.hpp"

namespace asset_discovery::parser {

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

} // namespace asset_discovery::parser
