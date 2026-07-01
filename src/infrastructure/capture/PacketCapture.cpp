#include "infrastructure/capture/PacketCapture.hpp"

#include "infrastructure/capture/AfPacketCaptureBackend.hpp"

#include <chrono>
#include <cstring>
#include <memory>
#include <sstream>
#include <thread>
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

PacketView OfflinePacket::view() const
{
    return {
        timestamp,
        linkType,
        capturedLength,
        originalLength,
        makeByteView(bytes),
        nullptr,
    };
}

bool PcapCaptureBackend::pcapAvailable() const
{
    return ASSET_DISCOVERY_HAS_PCAP == 1;
}

std::string PcapCaptureBackend::backendName() const
{
    return "pcap";
}

BackendAvailability PcapCaptureBackend::availability() const
{
    if (pcapAvailable()) {
        return {true, {}};
    }
    return {false, "libpcap backend is not available in this build"};
}

bool PcapCaptureBackend::supportsOfflinePcap() const
{
    return true;
}

PcapReadResult PcapCaptureBackend::readPcapFile(
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

std::optional<std::string> PcapCaptureBackend::captureLive(
    const CaptureConfig& config,
    LiveCaptureCallback callback,
    BackendStats* stats) const
{
#if ASSET_DISCOVERY_HAS_PCAP == 1
    const auto& interfaceName = config.interfaceName;
    const auto& options = config.liveOptions;
    const auto& packetFilter = config.packetFilter;

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
    const auto fillStats = [stats](pcap_t* pcapHandle) {
        if (stats == nullptr || pcapHandle == nullptr) {
            return;
        }

        pcap_stat pcapStats = {};
        if (pcap_stats(pcapHandle, &pcapStats) == 0) {
            stats->available = true;
            if (stats->selectedBackend.empty()) {
                stats->selectedBackend = "pcap";
            }
            stats->packetsReceived = static_cast<std::uint64_t>(pcapStats.ps_recv);
            stats->packetsDropped = static_cast<std::uint64_t>(pcapStats.ps_drop);
            stats->packetsInterfaceDropped = static_cast<std::uint64_t>(pcapStats.ps_ifdrop);
        }
    };

    const int datalink = pcap_datalink(handle);
    const auto linkType = normalizeDatalink(datalink);
    if (!linkType.has_value()) {
        const auto error = unsupportedLinkTypeError(interfaceName, datalink);
        fillStats(handle);
        closeHandle(handle);
        return error;
    }

    const auto filterError = applyPacketFilter(handle, interfaceName, packetFilter);
    if (filterError.has_value()) {
        fillStats(handle);
        closeHandle(handle);
        return filterError;
    }

    if (pcap_setnonblock(handle, 1, errorBuffer) == -1) {
        std::ostringstream output;
        output << "could not enable non-blocking capture for '" << interfaceName << "'";
        if (std::strlen(errorBuffer) > 0) {
            output << ": " << errorBuffer;
        }
        closeHandle(handle);
        return output.str();
    }

    const auto startTime = std::chrono::steady_clock::now();
    auto lastAcceptedPacketTime = startTime;

    pcap_pkthdr* header = nullptr;
    const unsigned char* data = nullptr;
    while (true) {
        if (options.stopRequested && options.stopRequested()) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        if (options.durationSeconds.has_value()) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed >= *options.durationSeconds) {
                break;
            }
        }

        if (options.idleTimeoutSeconds.has_value()) {
            const auto idleElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastAcceptedPacketTime).count();
            if (idleElapsed >= *options.idleTimeoutSeconds) {
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

            auto ownedBytes = std::make_shared<std::vector<std::uint8_t>>(packet.bytes);
            PacketView view;
            view.timestamp = packet.timestamp;
            view.linkType = packet.linkType;
            view.capturedLength = packet.capturedLength;
            view.originalLength = packet.originalLength;
            view.bytes = makeByteView(*ownedBytes);
            view.owner = ownedBytes;
            if (stats != nullptr) {
                ++stats->packetsCopied;
            }

            lastAcceptedPacketTime = std::chrono::steady_clock::now();
            callback(view);
            continue;
        }

        if (status == 0) {
            // Non-blocking read found no packet; avoid a hot loop while stop policy checks continue to run.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
        fillStats(handle);
        closeHandle(handle);
        return output.str();
    }

    fillStats(handle);
    closeHandle(handle);
    return std::nullopt;
#else
    (void)config;
    (void)callback;
    (void)stats;
    return "cannot run live capture: libpcap backend is not available in this build";
#endif
}

CaptureBackendFactoryResult createCaptureBackend(CaptureBackendSelection requested)
{
    CaptureBackendFactoryResult result;
    result.initialStats.requestedBackend = captureBackendSelectionName(requested);

    if (requested == CaptureBackendSelection::Pcap) {
        auto backend = std::make_unique<PcapCaptureBackend>();
        const auto availability = backend->availability();
        if (!availability.available) {
            result.error = availability.reason;
            return result;
        }
        result.initialStats.selectedBackend = backend->backendName();
        result.backend = std::move(backend);
        return result;
    }

    if (requested == CaptureBackendSelection::AfPacket) {
        auto backend = std::make_unique<AfPacketCaptureBackend>();
        const auto availability = backend->availability();
        if (!availability.available) {
            result.error = availability.reason;
            return result;
        }
        result.initialStats.selectedBackend = backend->backendName();
        result.backend = std::move(backend);
        return result;
    }

    auto afPacket = std::make_unique<AfPacketCaptureBackend>();
    const auto afPacketAvailability = afPacket->availability();
    if (afPacketAvailability.available) {
        result.initialStats.selectedBackend = afPacket->backendName();
        result.backend = std::move(afPacket);
        return result;
    }

    auto pcap = std::make_unique<PcapCaptureBackend>();
    const auto pcapAvailability = pcap->availability();
    if (pcapAvailability.available) {
        result.initialStats.selectedBackend = pcap->backendName();
        result.initialStats.fallbackReason = afPacketAvailability.reason;
        result.backend = std::move(pcap);
        return result;
    }

    result.error = "no capture backend is available";
    if (!afPacketAvailability.reason.empty() || !pcapAvailability.reason.empty()) {
        result.error = "no capture backend is available";
        if (!afPacketAvailability.reason.empty()) {
            *result.error += "; af-packet: " + afPacketAvailability.reason;
        }
        if (!pcapAvailability.reason.empty()) {
            *result.error += "; pcap: " + pcapAvailability.reason;
        }
    }
    return result;
}

std::string linkTypeName(LinkType linkType)
{
    switch (linkType) {
    case LinkType::Ethernet:
        return "ethernet";
    }
    return "unknown";
}

std::string captureBackendSelectionName(CaptureBackendSelection selection)
{
    switch (selection) {
    case CaptureBackendSelection::Auto:
        return "auto";
    case CaptureBackendSelection::Pcap:
        return "pcap";
    case CaptureBackendSelection::AfPacket:
        return "af-packet";
    }
    return "auto";
}

std::optional<CaptureBackendSelection> parseCaptureBackendSelection(const std::string& value)
{
    if (value == "auto") {
        return CaptureBackendSelection::Auto;
    }
    if (value == "pcap") {
        return CaptureBackendSelection::Pcap;
    }
    if (value == "af-packet") {
        return CaptureBackendSelection::AfPacket;
    }
    return std::nullopt;
}

} // namespace asset_discovery::capture
