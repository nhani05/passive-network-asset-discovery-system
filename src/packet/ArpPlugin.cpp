#include "pnad/packet/ArpPlugin.hpp"

#include "pnad/packet/ArpPacket.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace asset_discovery::parser {
namespace {

std::string normalizedMac(std::string macAddress)
{
    std::transform(macAddress.begin(), macAddress.end(), macAddress.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return macAddress;
}

std::string arpOperationName(std::uint16_t operation)
{
    if (operation == 1) {
        return "request";
    }
    if (operation == 2) {
        return "reply";
    }
    return "unknown";
}

std::string boolString(bool value)
{
    return value ? "true" : "false";
}

} // namespace

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
    addObservedMetadata(observation, "ethernet.source_mac", context.ethernet->sourceMac);
    addObservedMetadata(observation, "arp.sender_mac", arp.packet->senderMac);
    addObservedMetadata(observation, "arp.sender_ip", arp.packet->senderIp);
    addObservedMetadata(observation, "arp.target_mac", arp.packet->targetMac);
    addObservedMetadata(observation, "arp.target_ip", arp.packet->targetIp);
    addObservedMetadata(observation, "arp.operation", std::to_string(arp.packet->operation));
    addObservedMetadata(observation, "arp.operation_name", arpOperationName(arp.packet->operation));

    const bool gratuitous = arp.packet->senderIp == arp.packet->targetIp && arp.packet->senderIp != "0.0.0.0";
    const bool probe = arp.packet->senderIp == "0.0.0.0" && arp.packet->targetIp != "0.0.0.0";
    const bool announcement = arp.packet->operation == 1 && gratuitous;
    const bool macMismatch = normalizedMac(context.ethernet->sourceMac) != normalizedMac(arp.packet->senderMac);
    addObservedMetadata(observation, "arp.is_gratuitous", boolString(gratuitous));
    addObservedMetadata(observation, "arp.is_probe", boolString(probe));
    addObservedMetadata(observation, "arp.is_announcement", boolString(announcement));
    addObservedMetadata(observation, "arp.ethernet_sender_mac_mismatch", boolString(macMismatch));
    return {std::move(observation)};
}

} // namespace asset_discovery::parser
