#include "pnad/packet/Ipv4EndpointPlugin.hpp"

#include <optional>
#include <string>
#include <utility>

namespace asset_discovery::parser {
namespace {

std::optional<std::string> ttlOsHint(std::uint8_t ttl)
{
    if (ttl == 0) {
        return std::nullopt;
    }
    if (ttl <= 64) {
        return "linux/unix";
    }
    if (ttl <= 128) {
        return "windows";
    }
    return "network-device";
}

bool isSpecializedUdpPort(const UdpDatagram& udp)
{
    constexpr std::uint16_t dhcpServerPort = 67;
    constexpr std::uint16_t dhcpClientPort = 68;
    constexpr std::uint16_t dnsPort = 53;
    constexpr std::uint16_t ssdpPort = 1900;
    constexpr std::uint16_t mdnsPort = 5353;
    constexpr std::uint16_t llmnrPort = 5355;
    constexpr std::uint16_t netbiosNameServicePort = 137;
    return udp.sourcePort == dhcpServerPort || udp.destinationPort == dhcpServerPort
        || udp.sourcePort == dhcpClientPort || udp.destinationPort == dhcpClientPort
        || udp.sourcePort == dnsPort || udp.destinationPort == dnsPort
        || udp.sourcePort == ssdpPort || udp.destinationPort == ssdpPort
        || udp.sourcePort == mdnsPort || udp.destinationPort == mdnsPort
        || udp.sourcePort == llmnrPort || udp.destinationPort == llmnrPort
        || udp.sourcePort == netbiosNameServicePort || udp.destinationPort == netbiosNameServicePort;
}

} // namespace

std::string Ipv4EndpointPlugin::id() const
{
    return sourceIdIp;
}

ParserMatch Ipv4EndpointPlugin::match(const PacketContext& context) const
{
    if (context.ethernet.has_value() && context.ipv4.has_value()
        && (!context.udp.has_value() || !isSpecializedUdpPort(*context.udp))) {
        return {10};
    }
    return {};
}

std::vector<AssetObservation> Ipv4EndpointPlugin::parse(const PacketContext& context) const
{
    if (!match(context).matched() || !context.ethernet.has_value() || !context.ipv4.has_value()) {
        return {};
    }

    AssetObservation observation;
    observation.macAddress = context.ethernet->sourceMac;
    if (context.ipv4->sourceIp != "0.0.0.0") {
        observation.ipAddress = context.ipv4->sourceIp;
    }
    observation.sourceId = sourceIdIp;
    observation.eventType = ObservationEventType::Seen;
    observation.confidence = 0.4F;
    observation.timestamp = context.timestamp;
    addObservedMetadata(observation, "ipv4.ttl", std::to_string(context.ipv4->ttl));
    const auto hint = ttlOsHint(context.ipv4->ttl);
    if (hint.has_value()) {
        observation.osHint = *hint;
    }
    return {std::move(observation)};
}

} // namespace asset_discovery::parser
