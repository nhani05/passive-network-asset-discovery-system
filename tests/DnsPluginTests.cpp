#include "pnad/discovery/AssetObservation.hpp"
#include "pnad/packet/PacketParserFacade.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::parser::ObservationEventType;
using asset_discovery::parser::parseEthernetObservations;
using asset_discovery::parser::sourceIdDns;

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

    expect(observations.size() == 1, "valid DNS packet should create one endpoint observation");
    if (observations.empty()) {
        return;
    }

    const auto& observation = observations.front();
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

    expect(observations.empty(), "truncated DNS header should be skipped");
}

void skipsNonDnsUdpPacket()
{
    const std::vector<std::uint8_t> payload(12, 0x00);
    const auto observations = parseEthernetObservations(ipv4UdpFrame(123, 124, payload), {});

    expect(observations.empty(), "non-DNS UDP packet should be skipped");
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
    skipsMalformedFrame();

    if (failures > 0) {
        std::cerr << failures << " DNS plugin test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
