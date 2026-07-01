#include "pnad/packet/ParserEngine.hpp"

#include "pnad/packet/PacketContext.hpp"

#include <iterator>
#include <utility>

namespace asset_discovery::parser {

ParserEngine::ParserEngine(ParserRegistry registry)
    : registry_(std::move(registry))
{
}

std::vector<AssetObservation> ParserEngine::parse(const PacketContext& context) const
{
    std::vector<AssetObservation> observations;
    for (const auto& parser : registry_.parsers()) {
        if (!parser->match(context).matched()) {
            continue;
        }

        auto parsed = parser->parse(context);
        observations.insert(
            observations.end(),
            std::make_move_iterator(parsed.begin()),
            std::make_move_iterator(parsed.end()));
    }
    return observations;
}

std::vector<AssetObservation> ParserEngine::parse(
    ByteView bytes,
    ObservationTimestamp timestamp) const
{
    return parse(buildPacketContext(bytes, timestamp));
}

std::vector<AssetObservation> ParserEngine::parse(
    const std::vector<std::uint8_t>& bytes,
    ObservationTimestamp timestamp) const
{
    return parse(makeByteView(bytes), timestamp);
}

} // namespace asset_discovery::parser
