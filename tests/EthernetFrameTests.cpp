#include "pnad/packet/EthernetFrame.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::parser::EthernetDecodeError;
using asset_discovery::parser::decodeEthernetFrame;
using asset_discovery::parser::ethernetHeaderLength;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

std::vector<std::uint8_t> arpEthernetFrameFixture()
{
    return {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x02, 0x42, 0xac, 0x11, 0x00, 0x02,
        0x08, 0x06,
        0x00, 0x01, 0x08, 0x00, 0x06, 0x04,
    };
}

void decodesArpEthernetFrame()
{
    const auto frame = arpEthernetFrameFixture();
    const auto result = decodeEthernetFrame(frame);

    expect(result.ok(), "ARP Ethernet frame should decode successfully");
    expect(result.frame.has_value(), "ARP Ethernet frame should produce a frame");
    if (!result.frame.has_value()) {
        return;
    }

    expect(result.frame->destinationMac == "ff:ff:ff:ff:ff:ff", "destination MAC should decode");
    expect(result.frame->sourceMac == "02:42:ac:11:00:02", "source MAC should decode");
    expect(result.frame->etherType == 0x0806, "EtherType should decode as ARP");
    expect(result.frame->payload.size == 6, "payload should begin after the Ethernet header");
    expect(result.frame->payload[0] == 0x00, "first payload byte should be preserved");
    expect(result.frame->payload[result.frame->payload.size - 1] == 0x04, "last payload byte should be preserved");
}

void rejectsTruncatedFrame()
{
    const std::vector<std::uint8_t> frame(ethernetHeaderLength - 1, 0x00);
    const auto result = decodeEthernetFrame(frame);

    expect(!result.ok(), "truncated frame should not decode successfully");
    expect(!result.frame.has_value(), "truncated frame should not produce a frame");
    expect(result.error == EthernetDecodeError::TruncatedFrame, "truncated frame should return a controlled error");
}

void decodesUnsupportedEtherType()
{
    auto frame = arpEthernetFrameFixture();
    frame[12] = 0x88;
    frame[13] = 0xb5;

    const auto result = decodeEthernetFrame(frame);

    expect(result.ok(), "unsupported EtherType should still decode the Ethernet layer");
    expect(result.frame.has_value(), "unsupported EtherType should still produce a frame");
    if (!result.frame.has_value()) {
        return;
    }

    expect(result.frame->sourceMac == "02:42:ac:11:00:02", "unsupported EtherType should preserve source MAC");
    expect(result.frame->destinationMac == "ff:ff:ff:ff:ff:ff", "unsupported EtherType should preserve destination MAC");
    expect(result.frame->etherType == 0x88b5, "unsupported EtherType value should be preserved");
    expect(result.frame->payload.size == 6, "unsupported EtherType payload should be preserved");
}

void decodesHeaderOnlyFrame()
{
    const std::vector<std::uint8_t> frame = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x08, 0x00,
    };

    const auto result = decodeEthernetFrame(frame);

    expect(result.ok(), "header-only Ethernet frame should decode successfully");
    expect(result.frame.has_value(), "header-only Ethernet frame should produce a frame");
    if (!result.frame.has_value()) {
        return;
    }

    expect(result.frame->destinationMac == "01:02:03:04:05:06", "header-only frame destination MAC should decode");
    expect(result.frame->sourceMac == "0a:0b:0c:0d:0e:0f", "header-only frame source MAC should decode");
    expect(result.frame->etherType == 0x0800, "header-only frame EtherType should decode");
    expect(result.frame->payload.empty(), "header-only Ethernet frame should have an empty payload");
}

} // namespace

int main()
{
    decodesArpEthernetFrame();
    rejectsTruncatedFrame();
    decodesUnsupportedEtherType();
    decodesHeaderOnlyFrame();

    if (failures > 0) {
        std::cerr << failures << " Ethernet frame test expectation(s) failed\n";
        return 1;
    }

    return 0;
}
