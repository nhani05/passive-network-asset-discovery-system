#include "pnad/packet/PacketParserFacade.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::parser::parseEthernetObservations;
using asset_discovery::parser::ObservationEventType;
using asset_discovery::parser::sourceIdArp;
using asset_discovery::parser::sourceIdDhcp;
using asset_discovery::parser::sourceIdSsdp;

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
    const std::string vendorClass = "MSFT 5.0";
    bytes.push_back(60);
    bytes.push_back(static_cast<std::uint8_t>(vendorClass.size()));
    bytes.insert(bytes.end(), vendorClass.begin(), vendorClass.end());
    const std::vector<std::uint8_t> parameterRequestList = {1, 3, 6, 15, 31, 33, 43, 44, 46, 47, 119, 121, 249, 252};
    bytes.push_back(55);
    bytes.push_back(static_cast<std::uint8_t>(parameterRequestList.size()));
    bytes.insert(bytes.end(), parameterRequestList.begin(), parameterRequestList.end());
    bytes.push_back(50);
    bytes.push_back(4);
    bytes.push_back(192);
    bytes.push_back(168);
    bytes.push_back(1);
    bytes.push_back(20);
    bytes.push_back(255);
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
        0x00, 0x1a, 0x11, 0x22, 0x33, 0x44,
        0x08, 0x00,
        0x45, 0x00,
    };
    append16(bytes, ipLength);
    append16(bytes, 0);
    append16(bytes, 0);
    bytes.push_back(64);
    bytes.push_back(17);
    append16(bytes, 0);
    bytes.insert(bytes.end(), {192, 168, 1, 80, 239, 255, 255, 250});
    append16(bytes, sourcePort);
    append16(bytes, destinationPort);
    append16(bytes, udpLength);
    append16(bytes, 0);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
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
    const auto found = std::find_if(observations.begin(), observations.end(), [](const auto& observation) {
        return observation.sourceId == sourceIdDhcp;
    });
    expect(found != observations.end(), "DHCP frame should create a DHCP observation");
    if (found == observations.end()) {
        return;
    }
    const auto& observation = *found;
    expect(observation.macAddress == "02:42:ac:11:00:03", "client MAC should be parsed");
    expect(observation.ipAddress == "192.168.1.20", "requested IP should be parsed");
    expect(observation.hostname == "laptop-user", "hostname option should be parsed");
    expect(observation.displayName == "laptop-user", "hostname option should contribute display name");
    expect(observation.osHint == "windows", "DHCP option 55/60 should contribute OS hint");
    expect(observation.eventType == ObservationEventType::Update, "DHCP event should be an update");
    expect(observation.confidence == 1.0F, "DHCP confidence should preserve existing behavior");
    expect(observation.metadata.count("dhcp.option.hostname") == 1, "DHCP metadata should include hostname option");
}

void parsesSsdpSummary()
{
    const std::string payload =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "NT: urn:schemas-upnp-org:device:MediaRenderer:1\r\n"
        "SERVER: Linux/5.10 UPnP/1.0 DemoTV/1.0\r\n"
        "USN: uuid:demo::urn:schemas-upnp-org:device:MediaRenderer:1\r\n"
        "MANUFACTURER: Samsung\r\n"
        "MODELNAME: Smart TV\r\n"
        "\r\n";
    const auto observations = parseEthernetObservations(
        ipv4UdpFrame(1900, 1900, {payload.begin(), payload.end()}), {102, 0});
    const auto found = std::find_if(observations.begin(), observations.end(), [](const auto& observation) {
        return observation.sourceId == sourceIdSsdp;
    });
    expect(found != observations.end(), "SSDP packet should create SSDP observation");
    if (found == observations.end()) {
        return;
    }
    expect(found->vendor == "Samsung", "SSDP manufacturer should contribute vendor");
    expect(found->deviceType == "media-renderer", "SSDP NT should classify media renderer");
    expect(found->modelHint == "Smart TV", "SSDP model name should contribute model hint");
    expect(found->osHint == "linux", "SSDP server should contribute OS hint");
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
    parsesSsdpSummary();
    parsesArpMetadata();
    skipsTruncatedIpv4();

    if (failures > 0) {
        std::cerr << failures << " packet parser test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
