#include "parser/EthernetFrame.hpp"

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
        std::cerr << "THẤT BẠI: " << message << "\n";
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
    const auto result = decodeEthernetFrame(arpEthernetFrameFixture());

    expect(result.ok(), "frame Ethernet ARP phải giải mã thành công");
    expect(result.frame.has_value(), "frame Ethernet ARP phải tạo ra frame");
    if (!result.frame.has_value()) {
        return;
    }

    expect(result.frame->destinationMac == "ff:ff:ff:ff:ff:ff", "MAC đích phải được giải mã");
    expect(result.frame->sourceMac == "02:42:ac:11:00:02", "MAC nguồn phải được giải mã");
    expect(result.frame->etherType == 0x0806, "EtherType phải được giải mã là ARP");
    expect(result.frame->payload.size() == 6, "payload phải bắt đầu sau header Ethernet");
    expect(result.frame->payload.front() == 0x00, "byte đầu của payload phải được giữ nguyên");
    expect(result.frame->payload.back() == 0x04, "byte cuối của payload phải được giữ nguyên");
}

void rejectsTruncatedFrame()
{
    const std::vector<std::uint8_t> frame(ethernetHeaderLength - 1, 0x00);
    const auto result = decodeEthernetFrame(frame);

    expect(!result.ok(), "frame thiếu dữ liệu không được giải mã thành công");
    expect(!result.frame.has_value(), "frame thiếu dữ liệu không được tạo ra frame");
    expect(result.error == EthernetDecodeError::TruncatedFrame, "frame thiếu dữ liệu phải trả về lỗi có kiểm soát");
}

void decodesUnsupportedEtherType()
{
    auto frame = arpEthernetFrameFixture();
    frame[12] = 0x88;
    frame[13] = 0xb5;

    const auto result = decodeEthernetFrame(frame);

    expect(result.ok(), "EtherType chưa hỗ trợ vẫn phải giải mã được lớp Ethernet");
    expect(result.frame.has_value(), "EtherType chưa hỗ trợ vẫn phải tạo ra frame");
    if (!result.frame.has_value()) {
        return;
    }

    expect(result.frame->sourceMac == "02:42:ac:11:00:02", "EtherType chưa hỗ trợ vẫn giữ MAC nguồn");
    expect(result.frame->destinationMac == "ff:ff:ff:ff:ff:ff", "EtherType chưa hỗ trợ vẫn giữ MAC đích");
    expect(result.frame->etherType == 0x88b5, "giá trị EtherType chưa hỗ trợ phải được giữ nguyên");
    expect(result.frame->payload.size() == 6, "payload của EtherType chưa hỗ trợ phải được giữ nguyên");
}

void decodesHeaderOnlyFrame()
{
    const std::vector<std::uint8_t> frame = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x08, 0x00,
    };

    const auto result = decodeEthernetFrame(frame);

    expect(result.ok(), "frame Ethernet chỉ có header phải giải mã thành công");
    expect(result.frame.has_value(), "frame Ethernet chỉ có header phải tạo ra frame");
    if (!result.frame.has_value()) {
        return;
    }

    expect(result.frame->destinationMac == "01:02:03:04:05:06", "MAC đích của frame chỉ có header phải được giải mã");
    expect(result.frame->sourceMac == "0a:0b:0c:0d:0e:0f", "MAC nguồn của frame chỉ có header phải được giải mã");
    expect(result.frame->etherType == 0x0800, "EtherType của frame chỉ có header phải được giải mã");
    expect(result.frame->payload.empty(), "frame Ethernet chỉ có header phải có payload rỗng");
}

} // namespace

int main()
{
    decodesArpEthernetFrame();
    rejectsTruncatedFrame();
    decodesUnsupportedEtherType();
    decodesHeaderOnlyFrame();

    if (failures > 0) {
        std::cerr << failures << " kỳ vọng test Ethernet frame thất bại\n";
        return 1;
    }

    return 0;
}
