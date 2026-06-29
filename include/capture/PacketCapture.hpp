#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::capture {

enum class LinkType {
    Ethernet,
};

// Timestamp được giữ nguyên từ nguồn packet, không chuyển đổi timezone.
struct PacketTimestamp {
    std::int64_t seconds = 0;
    std::int64_t microseconds = 0;
};

// Dữ liệu packet thô do backend capture trả về.
struct OfflinePacket {
    PacketTimestamp timestamp;
    LinkType linkType = LinkType::Ethernet;
    std::uint32_t capturedLength = 0;
    std::uint32_t originalLength = 0;
    std::vector<std::uint8_t> bytes;
};

// Reader chỉ nên điền một trong hai trường: packets hoặc error.
struct PcapReadResult {
    std::vector<OfflinePacket> packets;
    std::optional<std::string> error;
};

class PacketCaptureBackend {
public:
    // Báo bản build hiện tại có liên kết backend libpcap dùng được hay không.
    bool pcapAvailable() const;
    std::string backendName() const;

    // Đọc toàn bộ file PCAP; lỗi được trả về thay vì ném exception.
    PcapReadResult readPcapFile(const std::string& path) const;

    using LiveCaptureCallback = std::function<void(const OfflinePacket&)>;

    // Lấy gói tin streaming trực tiếp từ interface.
    std::optional<std::string> captureLive(
        const std::string& interfaceName,
        std::optional<int> durationSeconds,
        LiveCaptureCallback callback) const;
};

std::string linkTypeName(LinkType linkType);

} // namespace asset_discovery::capture
