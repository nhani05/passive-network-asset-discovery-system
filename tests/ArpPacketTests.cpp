#include "parser/ArpPacket.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::parser::ArpDecodeError;
using asset_discovery::parser::ObservationSource;
using asset_discovery::parser::ObservationTimestamp;
using asset_discovery::parser::arpEthernetIpv4Length;
using asset_discovery::parser::decodeArpPacket;
using asset_discovery::parser::parseArpObservationsFromEthernetFrame;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "THẤT BẠI: " << message << "\n";
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

std::vector<std::uint8_t> ethernetFrameWithEtherType(std::uint16_t etherType)
{
    std::vector<std::uint8_t> frame = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x02, 0x42, 0xac, 0x11, 0x00, 0x02,
        static_cast<std::uint8_t>((etherType >> 8U) & 0xffU),
        static_cast<std::uint8_t>(etherType & 0xffU),
    };
    const auto payload = arpRequestPayloadFixture();
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

void decodesValidArpRequest()
{
    const auto result = decodeArpPacket(arpRequestPayloadFixture());

    expect(result.ok(), "ARP request hợp lệ phải giải mã thành công");
    expect(result.packet.has_value(), "ARP request hợp lệ phải tạo dữ liệu packet");
    if (!result.packet.has_value()) {
        return;
    }

    expect(result.packet->operation == 1, "operation ARP phải được giải mã là request");
    expect(result.packet->senderMac == "02:42:ac:11:00:02", "MAC gửi phải được giải mã");
    expect(result.packet->senderIp == "192.168.1.10", "IP gửi phải được giải mã");
    expect(result.packet->targetMac == "00:00:00:00:00:00", "MAC đích phải được giải mã");
    expect(result.packet->targetIp == "192.168.1.1", "IP đích phải được giải mã");
}

void emitsSenderObservation()
{
    const ObservationTimestamp timestamp{123, 456};
    const auto observations = parseArpObservationsFromEthernetFrame(ethernetFrameWithEtherType(0x0806), timestamp);

    expect(observations.size() == 1, "frame Ethernet ARP hợp lệ phải sinh một observation");
    if (observations.empty()) {
        return;
    }

    expect(observations.front().source == ObservationSource::Arp, "source của observation phải là ARP");
    expect(observations.front().macAddress == "02:42:ac:11:00:02", "MAC observation phải dùng MAC gửi");
    expect(observations.front().ipAddress == "192.168.1.10", "IP observation phải dùng IP gửi");
    expect(observations.front().timestamp.seconds == 123, "giây của observation phải giữ timestamp packet");
    expect(observations.front().timestamp.microseconds == 456, "micro giây của observation phải giữ timestamp packet");
}

void rejectsTruncatedArpPayload()
{
    const std::vector<std::uint8_t> payload(arpEthernetIpv4Length - 1, 0x00);
    const auto result = decodeArpPacket(payload);

    expect(!result.ok(), "payload ARP thiếu dữ liệu không được giải mã thành công");
    expect(!result.packet.has_value(), "payload ARP thiếu dữ liệu không được tạo dữ liệu packet");
    expect(result.error == ArpDecodeError::TruncatedPacket, "payload ARP thiếu dữ liệu phải trả về lỗi có kiểm soát");
}

void rejectsMalformedArpMetadata()
{
    auto payload = arpRequestPayloadFixture();
    payload[4] = 0x05;

    const auto result = decodeArpPacket(payload);

    expect(!result.ok(), "độ dài phần cứng ARP chưa hỗ trợ không được giải mã thành công");
    expect(result.error == ArpDecodeError::UnsupportedHardwareLength, "độ dài phần cứng chưa hỗ trợ phải được báo rõ");
}

void skipsNonArpEthernetFrame()
{
    const auto observations = parseArpObservationsFromEthernetFrame(ethernetFrameWithEtherType(0x0800), {});

    expect(observations.empty(), "frame Ethernet không phải ARP không được sinh observation ARP");
}

} // namespace

int main()
{
    decodesValidArpRequest();
    emitsSenderObservation();
    rejectsTruncatedArpPayload();
    rejectsMalformedArpMetadata();
    skipsNonArpEthernetFrame();

    if (failures > 0) {
        std::cerr << failures << " kỳ vọng test ARP packet thất bại\n";
        return 1;
    }

    return 0;
}
