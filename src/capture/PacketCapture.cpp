#include "capture/PacketCapture.hpp"

#include <chrono>
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

std::optional<std::string> applyPacketFilter(
    pcap_t* handle,
    const std::string& sourceName,
    const std::optional<std::string>& packetFilter)
{
    if (!packetFilter.has_value()) {
        return std::nullopt;
    }

    bpf_program compiledFilter = {};
    if (pcap_compile(handle, &compiledFilter, packetFilter->c_str(), 1, PCAP_NETMASK_UNKNOWN) != 0) {
        std::ostringstream output;
        output << "invalid BPF filter for '" << sourceName << "': " << pcap_geterr(handle);
        return output.str();
    }

    if (pcap_setfilter(handle, &compiledFilter) != 0) {
        std::ostringstream output;
        output << "could not apply BPF filter for '" << sourceName << "': " << pcap_geterr(handle);
        pcap_freecode(&compiledFilter);
        return output.str();
    }

    pcap_freecode(&compiledFilter);
    return std::nullopt;
}

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
    output << "PCAP link type " << datalink << " in '" << path << "' is not supported";
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

PcapReadResult PacketCaptureBackend::readPcapFile(
    const std::string& path,
    std::optional<std::string> packetFilter) const
{
#if ASSET_DISCOVERY_HAS_PCAP == 1
    // libpcap owns the handle; every exit path below must close it.
    char errorBuffer[PCAP_ERRBUF_SIZE] = {};
    pcap_t* handle = pcap_open_offline(path.c_str(), errorBuffer);
    if (handle == nullptr) {
        std::ostringstream output;
        output << "could not open PCAP file '" << path << "'";
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

    const auto filterError = applyPacketFilter(handle, path, packetFilter);
    if (filterError.has_value()) {
        closeHandle(handle);
        return {{}, *filterError};
    }

    std::vector<OfflinePacket> packets;
    pcap_pkthdr* header = nullptr;
    const unsigned char* data = nullptr;
    while (true) {
        const int status = pcap_next_ex(handle, &header, &data);
        if (status == 1) {
            // Defensive check: successful libpcap reads must return both pointers.
            if (header == nullptr || data == nullptr) {
                closeHandle(handle);
                return {{}, "could not read PCAP file '" + path + "': libpcap returned an empty packet"};
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
        output << "could not read PCAP file '" << path << "'";
        const char* pcapError = pcap_geterr(handle);
        if (pcapError != nullptr && std::strlen(pcapError) > 0) {
            output << ": " << pcapError;
        }
        closeHandle(handle);
        return {{}, output.str()};
    }

    closeHandle(handle);

    return {std::move(packets), std::nullopt};
#else
    return {{}, "cannot read PCAP file '" + path + "': libpcap backend is not available in this build"};
#endif
}

std::optional<std::string> PacketCaptureBackend::captureLive(
    const std::string& interfaceName,
    std::optional<int> durationSeconds,
    std::optional<std::string> packetFilter,
    LiveCaptureCallback callback) const
{
#if ASSET_DISCOVERY_HAS_PCAP == 1
    char errorBuffer[PCAP_ERRBUF_SIZE] = {};
    // Open the interface with snaplen 65535, promiscuous mode = 1, timeout = 1000ms.
    pcap_t* handle = pcap_open_live(interfaceName.c_str(), 65535, 1, 1000, errorBuffer);
    if (handle == nullptr) {
        std::ostringstream output;
        output << "could not open interface '" << interfaceName << "'";
        if (std::strlen(errorBuffer) > 0) {
            output << ": " << errorBuffer;
        }
        return output.str();
    }

    const auto closeHandle = [](pcap_t* pcapHandle) {
        if (pcapHandle != nullptr) {
            pcap_close(pcapHandle);
        }
    };

    const int datalink = pcap_datalink(handle);
    const auto linkType = normalizeDatalink(datalink);
    if (!linkType.has_value()) {
        const auto error = unsupportedLinkTypeError(interfaceName, datalink);
        closeHandle(handle);
        return error;
    }

    const auto filterError = applyPacketFilter(handle, interfaceName, packetFilter);
    if (filterError.has_value()) {
        closeHandle(handle);
        return filterError;
    }

    auto startTime = std::chrono::steady_clock::now();

    pcap_pkthdr* header = nullptr;
    const unsigned char* data = nullptr;
    while (true) {
        if (durationSeconds.has_value()) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed >= *durationSeconds) {
                break;
            }
        }

        const int status = pcap_next_ex(handle, &header, &data);
        if (status == 1) {
            if (header == nullptr || data == nullptr) {
                closeHandle(handle);
                return "could not read packet from '" + interfaceName + "': libpcap returned an empty packet";
            }

            OfflinePacket packet;
            packet.timestamp.seconds = static_cast<std::int64_t>(header->ts.tv_sec);
            packet.timestamp.microseconds = static_cast<std::int64_t>(header->ts.tv_usec);
            packet.linkType = *linkType;
            packet.capturedLength = header->caplen;
            packet.originalLength = header->len;
            packet.bytes.assign(data, data + header->caplen);

            callback(packet);
            continue;
        }

        if (status == 0) {
            // Packet read timeout; continue so the duration check can run.
            continue;
        }

        if (status == -2) {
            break;
        }

        std::ostringstream output;
        output << "error while reading interface '" << interfaceName << "'";
        const char* pcapError = pcap_geterr(handle);
        if (pcapError != nullptr && std::strlen(pcapError) > 0) {
            output << ": " << pcapError;
        }
        closeHandle(handle);
        return output.str();
    }

    closeHandle(handle);
    return std::nullopt;
#else
    return "cannot run live capture on '" + interfaceName + "': libpcap backend is not available in this build";
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
