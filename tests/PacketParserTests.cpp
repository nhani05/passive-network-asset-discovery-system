#include "application/parser/PacketParserFacade.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::parser::parseEthernetObservations;
using asset_discovery::parser::ObservationEventType;
using asset_discovery::parser::sourceIdArp;
using asset_discovery::parser::sourceIdDhcp;

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

void append32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

std::vector<std::uint8_t> dhcpPayload()
{
    std::vector<std::uint8_t> bytes(236, 0);
    bytes[0] = 1;
    bytes[1] = 1;
    bytes[2] = 6;
    bytes[3] = 0;
    bytes[28] = 0x02;
    bytes[29] = 0x42;
    bytes[30] = 0xac;
    bytes[31] = 0x11;
    bytes[32] = 0x00;
    bytes[33] = 0x03;
    append32(bytes, 0x63825363);
    bytes.push_back(53);
    bytes.push_back(1);
    bytes.push_back(1);
    bytes.push_back(12);
    bytes.push_back(11);
    const std::string hostname = "laptop-user";
    bytes.insert(bytes.end(), hostname.begin(), hostname.end());
    bytes.push_back(50);
    bytes.push_back(4);
    bytes.push_back(192);
    bytes.push_back(168);
    bytes.push_back(1);
    bytes.push_back(20);
    bytes.push_back(255);
    return bytes;
}

std::vector<std::uint8_t> dhcpEthernetFrame()
{
    const auto dhcp = dhcpPayload();
    const std::uint16_t udpLength = static_cast<std::uint16_t>(8 + dhcp.size());
    const std::uint16_t ipLength = static_cast<std::uint16_t>(20 + udpLength);

    std::vector<std::uint8_t> bytes = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x02, 0x42, 0xac, 0x11, 0x00, 0x03,
        0x08, 0x00,
        0x45, 0x00,
    };
    append16(bytes, ipLength);
    append16(bytes, 0);
    append16(bytes, 0);
    bytes.push_back(64);
    bytes.push_back(17);
    append16(bytes, 0);
    bytes.insert(bytes.end(), {0, 0, 0, 0, 255, 255, 255, 255});
    append16(bytes, 68);
    append16(bytes, 67);
    append16(bytes, udpLength);
    append16(bytes, 0);
    bytes.insert(bytes.end(), dhcp.begin(), dhcp.end());
    return bytes;
}

std::vector<std::uint8_t> arpEthernetFrame()
{
    std::vector<std::uint8_t> bytes = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x02, 0x42, 0xac, 0x11, 0x00, 0x02,
        0x08, 0x06,
    };
    append16(bytes, 1);
    append16(bytes, 0x0800);
    bytes.push_back(6);
    bytes.push_back(4);
    append16(bytes, 1);
    bytes.insert(bytes.end(), {0x02, 0x42, 0xac, 0x11, 0x00, 0x02});
    bytes.insert(bytes.end(), {192, 168, 1, 10});
    bytes.insert(bytes.end(), {0, 0, 0, 0, 0, 0});
    bytes.insert(bytes.end(), {192, 168, 1, 1});
    return bytes;
}

void parsesDhcpObservation()
{
    const auto observations = parseEthernetObservations(dhcpEthernetFrame(), {100, 200});
    expect(observations.size() == 1, "DHCP frame should create one observation");
    if (observations.empty()) {
        return;
    }
    expect(observations.front().sourceId == sourceIdDhcp, "source should be DHCP");
    expect(observations.front().macAddress == "02:42:ac:11:00:03", "client MAC should be parsed");
    expect(observations.front().ipAddress == "192.168.1.20", "requested IP should be parsed");
    expect(observations.front().hostname == "laptop-user", "hostname option should be parsed");
    expect(observations.front().eventType == ObservationEventType::Update, "DHCP event should be an update");
    expect(observations.front().confidence == 1.0F, "DHCP confidence should preserve existing behavior");
    expect(observations.front().metadata.count("dhcp.option.hostname") == 1, "DHCP metadata should include hostname option");
}

void parsesArpMetadata()
{
    const auto observations = parseEthernetObservations(arpEthernetFrame(), {101, 300});
    expect(observations.size() == 1, "ARP frame should create one observation");
    if (observations.empty()) {
        return;
    }
    const auto& observation = observations.front();
    expect(observation.sourceId == sourceIdArp, "source should be ARP");
    expect(observation.metadata.at("ethernet.source_mac") == "02:42:ac:11:00:02",
        "ARP metadata should include Ethernet source MAC");
    expect(observation.metadata.at("arp.sender_mac") == "02:42:ac:11:00:02",
        "ARP metadata should include sender MAC");
    expect(observation.metadata.at("arp.sender_ip") == "192.168.1.10",
        "ARP metadata should include sender IP");
    expect(observation.metadata.at("arp.target_mac") == "00:00:00:00:00:00",
        "ARP metadata should include target MAC");
    expect(observation.metadata.at("arp.target_ip") == "192.168.1.1",
        "ARP metadata should include target IP");
    expect(observation.metadata.at("arp.operation") == "1",
        "ARP metadata should include operation");
}

void skipsTruncatedIpv4()
{
    std::vector<std::uint8_t> bytes = {0, 1, 2};
    expect(parseEthernetObservations(bytes, {}).empty(), "truncated frames should be skipped safely");
}

} // namespace

int main()
{
    parsesDhcpObservation();
    parsesArpMetadata();
    skipsTruncatedIpv4();

    if (failures > 0) {
        std::cerr << failures << " packet parser test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
