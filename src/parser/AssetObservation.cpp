#include "parser/AssetObservation.hpp"

namespace asset_discovery::parser {

std::string observationSourceName(ObservationSource source)
{
    switch (source) {
    case ObservationSource::Arp:
        return "arp";
    }
    return "unknown";
}

} // namespace asset_discovery::parser
