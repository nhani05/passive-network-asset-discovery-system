#include "parser/AssetObservation.hpp"

namespace asset_discovery::parser {

std::string observationSourceName(ObservationSource source)
{
    switch (source) {
    case ObservationSource::Arp:
        return "arp";
    case ObservationSource::Dhcp:
        return "dhcp";
    }
    return "unknown";
}

} // namespace asset_discovery::parser
