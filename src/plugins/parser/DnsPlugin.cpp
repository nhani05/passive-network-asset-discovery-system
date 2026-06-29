#include "plugins/parser/DnsPlugin.hpp"

#include <string>
#include <utility>

namespace asset_discovery::parser {
namespace {

constexpr std::uint16_t dnsPort = 53;
constexpr std::size_t dnsHeaderLength = 12;
constexpr float dnsEndpointConfidence = 0.7F;

bool isDnsPort(const UdpDatagram& udp)
{
    return udp.sourcePort == dnsPort || udp.destinationPort == dnsPort;
}

} // namespace

std::string DnsPlugin::id() const
{
    return sourceIdDns;
}

ParserMatch DnsPlugin::match(const PacketContext& context) const
{
    if (context.udp.has_value() && isDnsPort(*context.udp) && context.udp->payload.size() >= dnsHeaderLength) {
        return {80};
    }
    return {};
}

std::vector<AssetObservation> DnsPlugin::parse(const PacketContext& context) const
{
    if (!match(context).matched() || !context.ethernet.has_value() || !context.ipv4.has_value() || !context.udp.has_value()) {
        return {};
    }

    AssetObservation observation;
    observation.macAddress = context.ethernet->sourceMac;
    observation.ipAddress = context.ipv4->sourceIp;
    observation.sourceId = sourceIdDns;
    observation.eventType = ObservationEventType::Seen;
    observation.confidence = dnsEndpointConfidence;
    observation.timestamp = context.timestamp;
    observation.metadata["udp.source_port"] = std::to_string(context.udp->sourcePort);
    observation.metadata["udp.destination_port"] = std::to_string(context.udp->destinationPort);
    observation.metadata["dns.observation"] = "endpoint";
    return {std::move(observation)};
}

} // namespace asset_discovery::parser
