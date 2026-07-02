#include "pnad/packet/SsdpPlugin.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <utility>

namespace asset_discovery::parser {
namespace {

constexpr std::uint16_t ssdpPort = 1900;

bool isSsdpPort(const UdpDatagram& udp)
{
    return udp.sourcePort == ssdpPort || udp.destinationPort == ssdpPort;
}

std::string trim(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string ssdpMetadataKey(const std::string& header)
{
    if (header == "usn") {
        return "ssdp.usn";
    }
    if (header == "st") {
        return "ssdp.search_target";
    }
    if (header == "nt") {
        return "ssdp.notification_type";
    }
    if (header == "server") {
        return "ssdp.server";
    }
    if (header == "location") {
        return "ssdp.location";
    }
    return {};
}

} // namespace

std::string SsdpPlugin::id() const
{
    return sourceIdSsdp;
}

ParserMatch SsdpPlugin::match(const PacketContext& context) const
{
    if (context.udp.has_value() && isSsdpPort(*context.udp) && !context.udp->payload.empty()) {
        return {75};
    }
    return {};
}

std::vector<AssetObservation> SsdpPlugin::parse(const PacketContext& context) const
{
    if (!match(context).matched() || !context.ethernet.has_value() || !context.ipv4.has_value() || !context.udp.has_value()) {
        return {};
    }

    AssetObservation observation;
    observation.macAddress = context.ethernet->sourceMac;
    observation.ipAddress = context.ipv4->sourceIp;
    observation.sourceId = sourceIdSsdp;
    observation.eventType = ObservationEventType::Seen;
    observation.confidence = 0.9F;
    observation.timestamp = context.timestamp;
    addObservedMetadata(observation, "udp.source_port", std::to_string(context.udp->sourcePort));
    addObservedMetadata(observation, "udp.destination_port", std::to_string(context.udp->destinationPort));

    const std::string payload(context.udp->payload.begin(), context.udp->payload.end());
    std::istringstream input(payload);
    std::string line;
    bool firstLine = true;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        if (firstLine) {
            addObservedMetadata(observation, "ssdp.start_line", line);
            firstLine = false;
            continue;
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const auto header = lower(trim(line.substr(0, colon)));
        const auto value = trim(line.substr(colon + 1));
        const auto key = ssdpMetadataKey(header);
        if (!key.empty()) {
            addObservedMetadata(observation, key, value);
        }
    }

    return {std::move(observation)};
}

} // namespace asset_discovery::parser
