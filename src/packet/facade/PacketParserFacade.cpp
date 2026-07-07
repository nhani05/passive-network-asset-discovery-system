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

const ParserEngine& coreParserEngine()
{
    static const ParserEngine engine(createCoreParserRegistry());
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

std::vector<AssetObservation> parseCoreEthernetObservations(
    ByteView bytes,
    ObservationTimestamp timestamp)
{
    return coreParserEngine().parse(bytes, timestamp);
}

std::vector<AssetObservation> parseCoreEthernetObservations(
    const std::vector<std::uint8_t>& bytes,
    ObservationTimestamp timestamp)
{
    return parseCoreEthernetObservations(makeByteView(bytes), timestamp);
}

} // namespace asset_discovery::parser
