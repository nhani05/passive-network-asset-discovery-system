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
    // Keep libpcap-specific datalink values out of the public capture model.
    if (datalink == ethernetDatalink) {
        return LinkType::Ethernet;
    }
    return std::nullopt;
}
#endif

std::string unsupportedLinkTypeError(const std::string& path, int datalink)
{
    std::ostringstream output;
    output << "unsupported PCAP link type " << datalink << " in '" << path << "'";
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
    // libpcap owns the handle; every exit after this point must close it.
    char errorBuffer[PCAP_ERRBUF_SIZE] = {};
    pcap_t* handle = pcap_open_offline(path.c_str(), errorBuffer);
    if (handle == nullptr) {
        std::ostringstream output;
        output << "failed to open PCAP file '" << path << "'";
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
            // Defensive check: successful libpcap reads should provide both pointers.
            if (header == nullptr || data == nullptr) {
                closeHandle(handle);
                return {{}, "failed to read PCAP file '" + path + "': libpcap returned an empty packet"};
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

        // pcap_next_ex returns -2 for offline EOF; other non-success states are errors.
        if (status == -2) {
            break;
        }

        std::ostringstream output;
        output << "failed to read PCAP file '" << path << "'";
        const char* pcapError = pcap_geterr(handle);
        if (pcapError != nullptr && std::strlen(pcapError) > 0) {
            output << ": " << pcapError;
        }
        closeHandle(handle);
        return {{}, output.str()};
    }

    closeHandle(handle);

    if (packets.empty()) {
        return {{}, "PCAP file '" + path + "' contains no packets"};
    }

    return {std::move(packets), std::nullopt};
#else
    return {{}, "cannot read PCAP file '" + path + "': libpcap backend is not available in this build"};
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
