#include "pnad/discovery/AssetObservation.hpp"
#include "pnad/packet/PacketParserFacade.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::parser::ObservationEventType;
using asset_discovery::parser::parseEthernetObservations;
using asset_discovery::parser::sourceIdDns;
using asset_discovery::parser::sourceIdMdns;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void append16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

std::vector<std::uint8_t> dnsQueryPayload()
{
    return {
        0x12, 0x34, 0x01, 0x00,
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x03, 'w', 'w', 'w',
        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
        0x03, 'c', 'o', 'm',
        0x00,
        0x00, 0x01,
        0x00, 0x01,
    };
}

void appendDnsName(std::vector<std::uint8_t>& bytes, const std::vector<std::string>& labels)
{
    for (const auto& label : labels) {
        bytes.push_back(static_cast<std::uint8_t>(label.size()));
        bytes.insert(bytes.end(), label.begin(), label.end());
    }
    bytes.push_back(0);
}

std::vector<std::uint8_t> mdnsResponsePayload()
{
    std::vector<std::uint8_t> bytes = {
        0x00, 0x00, 0x84, 0x00,
        0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x00,
    };

    appendDnsName(bytes, {"_airplay", "_tcp", "local"});
    append16(bytes, 12);
    append16(bytes, 1);
    bytes.insert(bytes.end(), {0, 0, 0, 120});
    std::vector<std::uint8_t> ptrRdata;
    appendDnsName(ptrRdata, {"Nam-iPhone", "_airplay", "_tcp", "local"});
    append16(bytes, static_cast<std::uint16_t>(ptrRdata.size()));
    bytes.insert(bytes.end(), ptrRdata.begin(), ptrRdata.end());

    appendDnsName(bytes, {"Nam-iPhone", "_airplay", "_tcp", "local"});
    append16(bytes, 16);
    append16(bytes, 1);
    bytes.insert(bytes.end(), {0, 0, 0, 120});
    const std::string txt = "model=iPhone";
    append16(bytes, static_cast<std::uint16_t>(txt.size() + 1));
    bytes.push_back(static_cast<std::uint8_t>(txt.size()));
    bytes.insert(bytes.end(), txt.begin(), txt.end());
    return bytes;
}

std::vector<std::uint8_t> ipv4UdpFrame(
    std::uint16_t sourcePort,
    std::uint16_t destinationPort,
    const std::vector<std::uint8_t>& payload)
{
    const std::uint16_t udpLength = static_cast<std::uint16_t>(8 + payload.size());
    const std::uint16_t ipLength = static_cast<std::uint16_t>(20 + udpLength);

    std::vector<std::uint8_t> bytes = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x02, 0x42, 0xac, 0x11, 0x00, 0x05,
        0x08, 0x00,
        0x45, 0x00,
    };
    append16(bytes, ipLength);
    append16(bytes, 0);
    append16(bytes, 0);
    bytes.push_back(64);
    bytes.push_back(17);
    append16(bytes, 0);
    bytes.insert(bytes.end(), {192, 168, 1, 50, 8, 8, 8, 8});
    append16(bytes, sourcePort);
    append16(bytes, destinationPort);
    append16(bytes, udpLength);
    append16(bytes, 0);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

void parsesValidDnsEndpoint()
{
    const auto observations = parseEthernetObservations(ipv4UdpFrame(5353, 53, dnsQueryPayload()), {100, 200});

    const auto found = std::find_if(observations.begin(), observations.end(), [](const auto& observation) {
        return observation.sourceId == sourceIdDns;
    });
    expect(found != observations.end(), "valid DNS packet should create one DNS endpoint observation");
    if (found == observations.end()) {
        return;
    }

    const auto& observation = *found;
    expect(observation.macAddress == "02:42:ac:11:00:05", "DNS observation should use Ethernet source MAC");
    expect(observation.ipAddress == "192.168.1.50", "DNS observation should use IPv4 source address");
    expect(observation.sourceId == sourceIdDns, "DNS observation should preserve dns source id");
    expect(observation.eventType == ObservationEventType::Seen, "DNS observation event should be Seen");
    expect(observation.confidence > 0.0F && observation.confidence <= 1.0F, "DNS confidence should be normalized");
    expect(!observation.hostname.has_value(), "DNS query names should not become asset hostnames");
    const auto destinationPort = observation.metadata.find("udp.destination_port");
    expect(destinationPort != observation.metadata.end(), "DNS metadata should preserve destination port key");
    if (destinationPort != observation.metadata.end()) {
        expect(destinationPort->second == "53", "DNS metadata should preserve destination port value");
    }
}

void skipsTruncatedDnsPayload()
{
    const std::vector<std::uint8_t> truncated(11, 0x00);
    const auto observations = parseEthernetObservations(ipv4UdpFrame(5353, 53, truncated), {});

    expect(std::none_of(observations.begin(), observations.end(), [](const auto& observation) {
        return observation.sourceId == sourceIdDns;
    }), "truncated DNS header should skip DNS observation");
}

void skipsNonDnsUdpPacket()
{
    const std::vector<std::uint8_t> payload(12, 0x00);
    const auto observations = parseEthernetObservations(ipv4UdpFrame(123, 124, payload), {});

    expect(std::none_of(observations.begin(), observations.end(), [](const auto& observation) {
        return observation.sourceId == sourceIdDns;
    }), "non-DNS UDP packet should skip DNS observation");
}

void parsesMdnsSummary()
{
    const auto observations = parseEthernetObservations(ipv4UdpFrame(5353, 5353, mdnsResponsePayload()), {101, 0});
    const auto found = std::find_if(observations.begin(), observations.end(), [](const auto& observation) {
        return observation.sourceId == sourceIdMdns;
    });
    expect(found != observations.end(), "mDNS response should create mDNS observation");
    if (found == observations.end()) {
        return;
    }
    expect(found->displayName == "Nam-iPhone", "mDNS PTR instance should contribute display name");
    expect(found->deviceType == "apple-media", "mDNS service should classify device");
    expect(found->modelHint == "iPhone", "mDNS TXT model should contribute model hint");
}

void skipsMalformedFrame()
{
    const auto observations = parseEthernetObservations({0x01, 0x02, 0x03}, {});

    expect(observations.empty(), "malformed Ethernet frame should be skipped safely");
}

} // namespace

int main()
{
    parsesValidDnsEndpoint();
    skipsTruncatedDnsPayload();
    skipsNonDnsUdpPacket();
    parsesMdnsSummary();
    skipsMalformedFrame();

    if (failures > 0) {
        std::cerr << failures << " DNS plugin test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
