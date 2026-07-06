#include "pnad/packet/DnsPlugin.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace asset_discovery::parser {
namespace {

constexpr std::uint16_t dnsPort = 53;
constexpr std::uint16_t mdnsPort = 5353;
constexpr std::uint16_t llmnrPort = 5355;
constexpr std::size_t dnsHeaderLength = 12;
constexpr float dnsEndpointConfidence = 0.7F;

struct ResourceRecord {
    std::string name;
    std::uint16_t type = 0;
    std::size_t rdataOffset = 0;
    std::size_t rdataLength = 0;
};

bool isDnsFamilyPort(const UdpDatagram& udp)
{
    return udp.sourcePort == dnsPort || udp.destinationPort == dnsPort
        || udp.sourcePort == mdnsPort || udp.destinationPort == mdnsPort
        || udp.sourcePort == llmnrPort || udp.destinationPort == llmnrPort;
}

std::uint16_t readBigEndianUInt16(ByteView bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
                                      | static_cast<std::uint16_t>(bytes[offset + 1]));
}

std::string typeName(std::uint16_t type)
{
    switch (type) {
    case 1:
        return "A";
    case 5:
        return "CNAME";
    case 12:
        return "PTR";
    case 16:
        return "TXT";
    case 28:
        return "AAAA";
    case 33:
        return "SRV";
    default:
        return std::to_string(type);
    }
}

std::optional<std::string> readDnsName(ByteView payload, std::size_t& offset)
{
    std::vector<std::string> labels;
    auto cursor = offset;
    bool jumped = false;
    std::size_t jumps = 0;

    while (cursor < payload.size) {
        const auto length = payload[cursor];
        if (length == 0) {
            ++cursor;
            if (!jumped) {
                offset = cursor;
            }
            return labels.empty() ? "." : [&labels]() {
                std::ostringstream name;
                for (std::size_t index = 0; index < labels.size(); ++index) {
                    if (index > 0) {
                        name << '.';
                    }
                    name << labels[index];
                }
                return name.str();
            }();
        }

        if ((length & 0xc0U) == 0xc0U) {
            if (cursor + 1 >= payload.size) {
                return std::nullopt;
            }
            const auto pointer = static_cast<std::size_t>(((length & 0x3fU) << 8U) | payload[cursor + 1]);
            if (pointer >= payload.size || ++jumps > 16) {
                return std::nullopt;
            }
            if (!jumped) {
                offset = cursor + 2;
                jumped = true;
            }
            cursor = pointer;
            continue;
        }

        if ((length & 0xc0U) != 0U) {
            return std::nullopt;
        }
        ++cursor;
        if (cursor + length > payload.size) {
            return std::nullopt;
        }
        labels.emplace_back(payload.begin() + static_cast<std::ptrdiff_t>(cursor),
            payload.begin() + static_cast<std::ptrdiff_t>(cursor + length));
        cursor += length;
        if (!jumped) {
            offset = cursor;
        }
    }

    return std::nullopt;
}

std::optional<ResourceRecord> readResourceRecord(ByteView payload, std::size_t& offset)
{
    auto name = readDnsName(payload, offset);
    if (!name.has_value() || offset + 10 > payload.size) {
        return std::nullopt;
    }

    ResourceRecord record;
    record.name = std::move(*name);
    record.type = readBigEndianUInt16(payload, offset);
    const auto rdataLength = readBigEndianUInt16(payload, offset + 8);
    offset += 10;
    if (offset + rdataLength > payload.size) {
        return std::nullopt;
    }
    record.rdataOffset = offset;
    record.rdataLength = rdataLength;
    offset += rdataLength;
    return record;
}

std::string formatIpv4Address(ByteView bytes, std::size_t offset)
{
    std::ostringstream output;
    for (std::size_t i = 0; i < 4; ++i) {
        if (i > 0) {
            output << '.';
        }
        output << static_cast<int>(bytes[offset + i]);
    }
    return output.str();
}

std::string formatIpv6Address(ByteView bytes, std::size_t offset)
{
    std::ostringstream output;
    output << std::hex << std::nouppercase;
    for (std::size_t i = 0; i < 8; ++i) {
        if (i > 0) {
            output << ':';
        }
        output << readBigEndianUInt16(bytes, offset + (i * 2));
    }
    return output.str();
}

bool containsServiceName(const std::string& value)
{
    return value.find("._tcp.local") != std::string::npos
        || value.find("._udp.local") != std::string::npos;
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::optional<std::string> mdnsInstanceName(const std::string& name)
{
    const auto marker = name.find("._");
    if (marker == std::string::npos || marker == 0) {
        return std::nullopt;
    }
    return name.substr(0, marker);
}

std::optional<std::string> mdnsDeviceType(const std::string& name)
{
    const auto value = lower(name);
    if (value.find("_ipp._tcp.local") != std::string::npos
        || value.find("_printer._tcp.local") != std::string::npos
        || value.find("_pdl-datastream._tcp.local") != std::string::npos) {
        return "printer";
    }
    if (value.find("_airplay._tcp.local") != std::string::npos
        || value.find("_raop._tcp.local") != std::string::npos) {
        return "apple-media";
    }
    if (value.find("_googlecast._tcp.local") != std::string::npos) {
        return "cast-device";
    }
    if (value.find("_hap._tcp.local") != std::string::npos) {
        return "smart-home";
    }
    if (value.find("_ssh._tcp.local") != std::string::npos
        || value.find("_smb._tcp.local") != std::string::npos) {
        return "computer";
    }
    return std::nullopt;
}

void applyMdnsNameSummary(AssetObservation& observation, const std::string& name)
{
    if (observation.sourceId != sourceIdMdns) {
        return;
    }
    const auto instance = mdnsInstanceName(name);
    if (instance.has_value()) {
        observation.displayName = *instance;
    } else if (name.size() > 6 && lower(name).rfind(".local") == name.size() - 6 && !containsServiceName(name)) {
        observation.displayName = name.substr(0, name.size() - 6);
    }
    const auto deviceType = mdnsDeviceType(name);
    if (deviceType.has_value()) {
        observation.deviceType = *deviceType;
    }
}

void applyMdnsTxtSummary(AssetObservation& observation, const std::string& text)
{
    if (observation.sourceId != sourceIdMdns) {
        return;
    }
    const auto separator = text.find('=');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= text.size()) {
        return;
    }
    const auto key = lower(text.substr(0, separator));
    const auto value = text.substr(separator + 1);
    if (key == "model" || key == "md" || key == "ty" || key == "product" || key == "rp") {
        observation.modelHint = value;
    } else if (key == "manufacturer" || key == "mfg") {
        observation.vendor = value;
    }
}

std::string sourceIdForPort(const UdpDatagram& udp)
{
    if (udp.sourcePort == dnsPort || udp.destinationPort == dnsPort) {
        return sourceIdDns;
    }
    if (udp.sourcePort == mdnsPort || udp.destinationPort == mdnsPort) {
        return sourceIdMdns;
    }
    if (udp.sourcePort == llmnrPort || udp.destinationPort == llmnrPort) {
        return sourceIdLlmnr;
    }
    return sourceIdDns;
}

void addProtocolSpecificName(AssetObservation& observation, const std::string& name)
{
    if (observation.sourceId == sourceIdMdns && containsServiceName(name)) {
        addObservedMetadata(observation, "mdns.services", name);
        applyMdnsNameSummary(observation, name);
    } else if (observation.sourceId == sourceIdLlmnr && name != ".") {
        addObservedMetadata(observation, "llmnr.names", name);
    } else if (observation.sourceId == sourceIdMdns && name != ".") {
        applyMdnsNameSummary(observation, name);
    }
}

void addTxtRecordMetadata(AssetObservation& observation, ByteView payload, const ResourceRecord& record)
{
    std::size_t cursor = record.rdataOffset;
    const auto end = record.rdataOffset + record.rdataLength;
    while (cursor < end) {
        const auto length = payload[cursor++];
        if (cursor + length > end) {
            return;
        }
        const std::string text(payload.begin() + static_cast<std::ptrdiff_t>(cursor),
            payload.begin() + static_cast<std::ptrdiff_t>(cursor + length));
        addObservedMetadata(observation, "dns.txt", text);
        applyMdnsTxtSummary(observation, text);
        cursor += length;
    }
}

void addResourceRecordMetadata(AssetObservation& observation, ByteView payload, const ResourceRecord& record)
{
    addObservedMetadata(observation, "dns.answer_names", record.name);
    addProtocolSpecificName(observation, record.name);

    if (record.type == 1 && record.rdataLength == 4) {
        addObservedMetadata(observation, "dns.answer_ips", formatIpv4Address(payload, record.rdataOffset));
    } else if (record.type == 28 && record.rdataLength == 16) {
        addObservedMetadata(observation, "dns.answer_ips", formatIpv6Address(payload, record.rdataOffset));
    } else if (record.type == 12) {
        auto rdataOffset = record.rdataOffset;
        const auto ptrName = readDnsName(payload, rdataOffset);
        if (ptrName.has_value()) {
            addObservedMetadata(observation, "dns.ptr_names", *ptrName);
            addProtocolSpecificName(observation, *ptrName);
        }
    } else if (record.type == 16) {
        addTxtRecordMetadata(observation, payload, record);
    } else if (record.type == 33 && record.rdataLength >= 7) {
        auto targetOffset = record.rdataOffset + 6;
        const auto targetName = readDnsName(payload, targetOffset);
        if (targetName.has_value()) {
            addObservedMetadata(observation, "dns.srv_targets", *targetName);
            addProtocolSpecificName(observation, *targetName);
        }
    }
}

std::optional<AssetObservation> parseDnsPayload(const PacketContext& context)
{
    if (!context.ethernet.has_value() || !context.ipv4.has_value() || !context.udp.has_value()) {
        return std::nullopt;
    }

    const auto payload = context.udp->payload;
    if (payload.size < dnsHeaderLength) {
        return std::nullopt;
    }

    const auto questionCount = readBigEndianUInt16(payload, 4);
    const auto answerCount = readBigEndianUInt16(payload, 6);
    const auto authorityCount = readBigEndianUInt16(payload, 8);
    const auto additionalCount = readBigEndianUInt16(payload, 10);

    AssetObservation observation;
    observation.macAddress = context.ethernet->sourceMac;
    observation.ipAddress = context.ipv4->sourceIp;
    observation.sourceId = sourceIdForPort(*context.udp);
    observation.eventType = ObservationEventType::Seen;
    observation.confidence = dnsEndpointConfidence;
    observation.timestamp = context.timestamp;
    addObservedMetadata(observation, "udp.source_port", std::to_string(context.udp->sourcePort));
    addObservedMetadata(observation, "udp.destination_port", std::to_string(context.udp->destinationPort));
    addObservedMetadata(observation, "dns.observation", "endpoint");
    addObservedMetadata(observation, "dns.question_count", std::to_string(questionCount));
    addObservedMetadata(observation, "dns.answer_count", std::to_string(answerCount));

    std::size_t offset = dnsHeaderLength;
    for (std::uint16_t index = 0; index < questionCount; ++index) {
        auto queryName = readDnsName(payload, offset);
        if (!queryName.has_value() || offset + 4 > payload.size) {
            return std::nullopt;
        }
        const auto queryType = readBigEndianUInt16(payload, offset);
        offset += 4;
        addObservedMetadata(observation, "dns.queries", *queryName);
        addObservedMetadata(observation, "dns.query_types", typeName(queryType));
        addProtocolSpecificName(observation, *queryName);
    }

    const auto recordCount = static_cast<std::uint32_t>(answerCount)
        + static_cast<std::uint32_t>(authorityCount)
        + static_cast<std::uint32_t>(additionalCount);
    for (std::uint32_t index = 0; index < recordCount; ++index) {
        const auto record = readResourceRecord(payload, offset);
        if (!record.has_value()) {
            return std::nullopt;
        }
        addResourceRecordMetadata(observation, payload, *record);
    }

    return observation;
}

} // namespace

std::string DnsPlugin::id() const
{
    return sourceIdDns;
}

ParserMatch DnsPlugin::match(const PacketContext& context) const
{
    if (context.udp.has_value() && isDnsFamilyPort(*context.udp) && context.udp->payload.size >= dnsHeaderLength) {
        return {80};
    }
    return {};
}

std::vector<AssetObservation> DnsPlugin::parse(const PacketContext& context) const
{
    if (!match(context).matched()) {
        return {};
    }

    const auto observation = parseDnsPayload(context);
    if (!observation.has_value()) {
        return {};
    }
    return {*observation};
}

} // namespace asset_discovery::parser
