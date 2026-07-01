#include "pnad/packet/PacketParserFacade.hpp"

#include "pnad/packet/ParserEngine.hpp"
#include "pnad/packet/BuiltinParserPlugins.hpp"

namespace asset_discovery::parser {
namespace {

const ParserEngine& defaultParserEngine()
{
    static const ParserEngine engine(createDefaultParserRegistry());
    return engine;
}

} // namespace

std::vector<AssetObservation> parseEthernetObservations(
    ByteView bytes,
    ObservationTimestamp timestamp)
{
    return defaultParserEngine().parse(bytes, timestamp);
}

std::vector<AssetObservation> parseEthernetObservations(
    const std::vector<std::uint8_t>& bytes,
    ObservationTimestamp timestamp)
{
    return parseEthernetObservations(makeByteView(bytes), timestamp);
}

} // namespace asset_discovery::parser
