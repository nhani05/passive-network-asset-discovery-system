#include "plugins/parser/ArpPlugin.hpp"

#include "infrastructure/packet/ArpPacket.hpp"

#include <string>
#include <utility>

namespace asset_discovery::parser {

std::string ArpPlugin::id() const
{
    return sourceIdArp;
}

ParserMatch ArpPlugin::match(const PacketContext& context) const
{
    if (context.ethernet.has_value() && context.ethernet->etherType == ethernetTypeArp) {
        return {100};
    }
    return {};
}

std::vector<AssetObservation> ArpPlugin::parse(const PacketContext& context) const
{
    if (!match(context).matched() || !context.ethernet.has_value()) {
        return {};
    }

    const auto arp = decodeArpPacket(context.ethernet->payload);
    if (!arp.ok() || !arp.packet.has_value()) {
        return {};
    }

    auto observation = observationFromArpPacket(*arp.packet, context.timestamp);
    observation.metadata["ethernet.source_mac"] = context.ethernet->sourceMac;
    observation.metadata["arp.sender_mac"] = arp.packet->senderMac;
    observation.metadata["arp.sender_ip"] = arp.packet->senderIp;
    observation.metadata["arp.target_mac"] = arp.packet->targetMac;
    observation.metadata["arp.target_ip"] = arp.packet->targetIp;
    observation.metadata["arp.operation"] = std::to_string(arp.packet->operation);
    return {std::move(observation)};
}

} // namespace asset_discovery::parser
