#include "application/parser/PacketParserFacade.hpp"

#include "application/parser/ParserEngine.hpp"
#include "plugins/parser/BuiltinParserPlugins.hpp"

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
