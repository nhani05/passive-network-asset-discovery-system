#include "infrastructure/packet/ArpPacket.hpp"
#include "application/parser/PacketContext.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::parser::buildPacketContext;
using asset_discovery::parser::ethernetTypeArp;

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

std::vector<std::uint8_t> ethernetHeader(std::uint16_t etherType)
{
    std::vector<std::uint8_t> bytes = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x02, 0x42, 0xac, 0x11, 0x00, 0x05,
    };
    append16(bytes, etherType);
    return bytes;
}

std::vector<std::uint8_t> arpFrame()
{
    auto bytes = ethernetHeader(ethernetTypeArp);
    bytes.insert(bytes.end(), {
        0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01,
        0x02, 0x42, 0xac, 0x11, 0x00, 0x05,
        192, 168, 1, 50,
        0, 0, 0, 0, 0, 0,
        192, 168, 1, 1,
    });
    return bytes;
}

std::vector<std::uint8_t> ipv4UdpFrame(
    std::uint16_t sourcePort,
    std::uint16_t destinationPort,
    const std::vector<std::uint8_t>& payload,
    std::uint16_t fragmentField = 0)
{
    const std::uint16_t udpLength = static_cast<std::uint16_t>(8 + payload.size());
    const std::uint16_t ipLength = static_cast<std::uint16_t>(20 + udpLength);

    auto bytes = ethernetHeader(0x0800);
    bytes.insert(bytes.end(), {0x45, 0x00});
    append16(bytes, ipLength);
    append16(bytes, 0);
    append16(bytes, fragmentField);
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

void exposesArpEthernetContext()
{
    const auto context = buildPacketContext(arpFrame(), {10, 20});

    expect(context.ethernet.has_value(), "ARP frame should expose Ethernet layer");
    expect(context.ethernet->etherType == ethernetTypeArp, "ARP frame should preserve EtherType");
    expect(context.rawBytes == arpFrame(), "packet context should preserve raw bytes");
    expect(context.timestamp.seconds == 10, "packet context should preserve timestamp seconds");
    expect(!context.ipv4.has_value(), "ARP frame should not expose IPv4 layer");
    expect(!context.udp.has_value(), "ARP frame should not expose UDP layer");
}

void exposesIpv4UdpContext()
{
    const auto context = buildPacketContext(ipv4UdpFrame(5353, 53, {1, 2, 3}), {30, 40});

    expect(context.ethernet.has_value(), "IPv4 UDP frame should expose Ethernet layer");
    expect(context.ipv4.has_value(), "IPv4 UDP frame should expose IPv4 layer");
    expect(context.udp.has_value(), "IPv4 UDP frame should expose UDP layer");
    if (!context.ipv4.has_value() || !context.udp.has_value()) {
        return;
    }

    expect(context.ipv4->sourceIp == "192.168.1.50", "IPv4 source should decode");
    expect(context.ipv4->destinationIp == "8.8.8.8", "IPv4 destination should decode");
    expect(context.udp->sourcePort == 5353, "UDP source port should decode");
    expect(context.udp->destinationPort == 53, "UDP destination port should decode");
    expect(context.transportPayload == std::vector<std::uint8_t>({1, 2, 3}), "transport payload should decode");
}

void keepsUnsupportedEtherTypeAtEthernetLayer()
{
    auto bytes = ethernetHeader(0x88b5);
    bytes.push_back(0x01);

    const auto context = buildPacketContext(bytes, {});

    expect(context.ethernet.has_value(), "unsupported EtherType should still expose Ethernet layer");
    expect(!context.ipv4.has_value(), "unsupported EtherType should not expose IPv4 layer");
    expect(!context.udp.has_value(), "unsupported EtherType should not expose UDP layer");
}

void skipsUdpForFragmentedIpv4()
{
    const auto context = buildPacketContext(ipv4UdpFrame(5353, 53, {1, 2, 3}, 0x2000), {});

    expect(context.ipv4.has_value(), "fragmented IPv4 should still expose IPv4 metadata");
    expect(context.ipv4->fragmented, "fragmented IPv4 should be marked fragmented");
    expect(!context.udp.has_value(), "fragmented IPv4 should not expose UDP datagram");
}

void skipsTruncatedIpv4Layer()
{
    auto bytes = ethernetHeader(0x0800);
    bytes.insert(bytes.end(), {0x45, 0x00, 0x00});

    const auto context = buildPacketContext(bytes, {});

    expect(context.ethernet.has_value(), "truncated IPv4 frame should still expose Ethernet layer");
    expect(!context.ipv4.has_value(), "truncated IPv4 frame should not expose IPv4 metadata");
    expect(!context.udp.has_value(), "truncated IPv4 frame should not expose UDP datagram");
}

} // namespace

int main()
{
    exposesArpEthernetContext();
    exposesIpv4UdpContext();
    keepsUnsupportedEtherTypeAtEthernetLayer();
    skipsUdpForFragmentedIpv4();
    skipsTruncatedIpv4Layer();

    if (failures > 0) {
        std::cerr << failures << " packet context test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
