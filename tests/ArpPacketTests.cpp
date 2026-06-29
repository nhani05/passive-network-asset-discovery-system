#include "infrastructure/packet/ArpPacket.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::parser::ArpDecodeError;
using asset_discovery::parser::ObservationEventType;
using asset_discovery::parser::ObservationTimestamp;
using asset_discovery::parser::arpEthernetIpv4Length;
using asset_discovery::parser::decodeArpPacket;
using asset_discovery::parser::observationFromArpPacket;
using asset_discovery::parser::sourceIdArp;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

std::vector<std::uint8_t> arpRequestPayloadFixture()
{
    return {
        0x00, 0x01,
        0x08, 0x00,
        0x06,
        0x04,
        0x00, 0x01,
        0x02, 0x42, 0xac, 0x11, 0x00, 0x02,
        0xc0, 0xa8, 0x01, 0x0a,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xc0, 0xa8, 0x01, 0x01,
    };
}

void decodesValidArpRequest()
{
    const auto result = decodeArpPacket(arpRequestPayloadFixture());

    expect(result.ok(), "valid ARP request should decode successfully");
    expect(result.packet.has_value(), "valid ARP request should produce packet data");
    if (!result.packet.has_value()) {
        return;
    }

    expect(result.packet->operation == 1, "ARP operation should decode as request");
    expect(result.packet->senderMac == "02:42:ac:11:00:02", "sender MAC should decode");
    expect(result.packet->senderIp == "192.168.1.10", "sender IP should decode");
    expect(result.packet->targetMac == "00:00:00:00:00:00", "target MAC should decode");
    expect(result.packet->targetIp == "192.168.1.1", "target IP should decode");
}

void emitsSenderObservation()
{
    const ObservationTimestamp timestamp{123, 456};
    const auto result = decodeArpPacket(arpRequestPayloadFixture());

    expect(result.ok(), "valid ARP request should decode before creating an observation");
    if (!result.packet.has_value()) {
        return;
    }

    const auto observation = observationFromArpPacket(*result.packet, timestamp);

    expect(observation.sourceId == sourceIdArp, "observation source should be ARP");
    expect(observation.eventType == ObservationEventType::Seen, "ARP event should be Seen");
    expect(observation.confidence == 1.0F, "ARP confidence should preserve existing behavior");
    expect(observation.macAddress == "02:42:ac:11:00:02", "observation MAC should use the sender MAC");
    expect(observation.ipAddress == "192.168.1.10", "observation IP should use the sender IP");
    expect(observation.timestamp.seconds == 123, "observation seconds should preserve the packet timestamp");
    expect(observation.timestamp.microseconds == 456, "observation microseconds should preserve the packet timestamp");
}

void rejectsTruncatedArpPayload()
{
    const std::vector<std::uint8_t> payload(arpEthernetIpv4Length - 1, 0x00);
    const auto result = decodeArpPacket(payload);

    expect(!result.ok(), "truncated ARP payload should not decode successfully");
    expect(!result.packet.has_value(), "truncated ARP payload should not produce packet data");
    expect(result.error == ArpDecodeError::TruncatedPacket, "truncated ARP payload should return a controlled error");
}

void rejectsMalformedArpMetadata()
{
    auto payload = arpRequestPayloadFixture();
    payload[4] = 0x05;

    const auto result = decodeArpPacket(payload);

    expect(!result.ok(), "unsupported ARP hardware length should not decode successfully");
    expect(result.error == ArpDecodeError::UnsupportedHardwareLength, "unsupported hardware length should be reported clearly");
}

} // namespace

int main()
{
    decodesValidArpRequest();
    emitsSenderObservation();
    rejectsTruncatedArpPayload();
    rejectsMalformedArpMetadata();

    if (failures > 0) {
        std::cerr << failures << " ARP packet test expectation(s) failed\n";
        return 1;
    }

    return 0;
}
