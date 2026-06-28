#include "capture/PacketCapture.hpp"

#include <cstring>
#include <sstream>
#include <utility>

#ifndef ASSET_DISCOVERY_HAS_PCAP
#define ASSET_DISCOVERY_HAS_PCAP 0
#endif

#if ASSET_DISCOVERY_HAS_PCAP == 1
#include <pcap.h>
#endif

namespace asset_discovery::capture {
namespace {

#if ASSET_DISCOVERY_HAS_PCAP == 1
constexpr int ethernetDatalink = DLT_EN10MB;

std::optional<LinkType> normalizeDatalink(int datalink)
{
    // Giữ giá trị datalink riêng của libpcap bên ngoài model capture công khai.
    if (datalink == ethernetDatalink) {
        return LinkType::Ethernet;
    }
    return std::nullopt;
}
#endif

std::string unsupportedLinkTypeError(const std::string& path, int datalink)
{
    std::ostringstream output;
    output << "loại liên kết PCAP " << datalink << " trong '" << path << "' không được hỗ trợ";
    return output.str();
}

} // namespace

bool PacketCaptureBackend::pcapAvailable() const
{
    return ASSET_DISCOVERY_HAS_PCAP == 1;
}

std::string PacketCaptureBackend::backendName() const
{
    return ASSET_DISCOVERY_HAS_PCAP == 1 ? "libpcap" : "libpcap-unavailable";
}

PcapReadResult PacketCaptureBackend::readPcapFile(const std::string& path) const
{
#if ASSET_DISCOVERY_HAS_PCAP == 1
    // libpcap sở hữu handle; mọi nhánh thoát sau đây đều phải đóng handle.
    char errorBuffer[PCAP_ERRBUF_SIZE] = {};
    pcap_t* handle = pcap_open_offline(path.c_str(), errorBuffer);
    if (handle == nullptr) {
        std::ostringstream output;
        output << "không mở được file PCAP '" << path << "'";
        if (std::strlen(errorBuffer) > 0) {
            output << ": " << errorBuffer;
        }
        return {{}, output.str()};
    }

    const auto closeHandle = [](pcap_t* pcapHandle) {
        if (pcapHandle != nullptr) {
            pcap_close(pcapHandle);
        }
    };

    const int datalink = pcap_datalink(handle);
    const auto linkType = normalizeDatalink(datalink);
    if (!linkType.has_value()) {
        const auto error = unsupportedLinkTypeError(path, datalink);
        closeHandle(handle);
        return {{}, error};
    }

    std::vector<OfflinePacket> packets;
    pcap_pkthdr* header = nullptr;
    const unsigned char* data = nullptr;
    while (true) {
        const int status = pcap_next_ex(handle, &header, &data);
        if (status == 1) {
            // Kiểm tra phòng thủ: libpcap đọc thành công phải trả về đủ hai con trỏ.
            if (header == nullptr || data == nullptr) {
                closeHandle(handle);
                return {{}, "không đọc được file PCAP '" + path + "': libpcap trả về packet rỗng"};
            }

            OfflinePacket packet;
            packet.timestamp.seconds = static_cast<std::int64_t>(header->ts.tv_sec);
            packet.timestamp.microseconds = static_cast<std::int64_t>(header->ts.tv_usec);
            packet.linkType = *linkType;
            packet.capturedLength = header->caplen;
            packet.originalLength = header->len;
            packet.bytes.assign(data, data + header->caplen);
            packets.push_back(std::move(packet));
            continue;
        }

        // pcap_next_ex trả -2 khi offline EOF; trạng thái không thành công khác là lỗi.
        if (status == -2) {
            break;
        }

        std::ostringstream output;
        output << "không đọc được file PCAP '" << path << "'";
        const char* pcapError = pcap_geterr(handle);
        if (pcapError != nullptr && std::strlen(pcapError) > 0) {
            output << ": " << pcapError;
        }
        closeHandle(handle);
        return {{}, output.str()};
    }

    closeHandle(handle);

    if (packets.empty()) {
        return {{}, "file PCAP '" + path + "' không có packet"};
    }

    return {std::move(packets), std::nullopt};
#else
    return {{}, "không thể đọc file PCAP '" + path + "': backend libpcap không có trong bản build này"};
#endif
}

std::string linkTypeName(LinkType linkType)
{
    switch (linkType) {
    case LinkType::Ethernet:
        return "ethernet";
    }
    return "unknown";
}

} // namespace asset_discovery::capture
