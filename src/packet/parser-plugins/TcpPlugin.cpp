#include "pnad/packet/TcpPlugin.hpp"

#include <string>
#include <utility>

namespace asset_discovery::parser {

std::string TcpPlugin::id() const
{
    return sourceIdTcp;
}

ParserMatch TcpPlugin::match(const PacketContext& context) const
{
    if (context.tcp.has_value() && context.tcp->syn && context.tcp->ack && !context.tcp->rst) {
        return {85};
    }
    return {};
}

std::vector<AssetObservation> TcpPlugin::parse(const PacketContext& context) const
{
    if (!match(context).matched() || !context.ethernet.has_value() || !context.ipv4.has_value() || !context.tcp.has_value()) {
        return {};
    }

    AssetObservation observation;
    observation.macAddress = context.ethernet->sourceMac;
    observation.ipAddress = context.ipv4->sourceIp;
    observation.sourceId = sourceIdTcp;
    observation.eventType = ObservationEventType::Seen;
    observation.confidence = 1.0F;
    observation.timestamp = context.timestamp;
    addObservedMetadata(observation, "tcp.source_port", std::to_string(context.tcp->sourcePort));
    addObservedMetadata(observation, "tcp.destination_port", std::to_string(context.tcp->destinationPort));
    addObservedMetadata(observation, "tcp.flags", "syn,ack");
    addObservedMetadata(observation, "tcp.syn_ack_ports", std::to_string(context.tcp->sourcePort));
    return {std::move(observation)};
}

} // namespace asset_discovery::parser
