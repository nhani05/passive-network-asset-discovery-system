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

// Timestamp is preserved from the packet source without timezone conversion.
struct PacketTimestamp {
    std::int64_t seconds = 0;
    std::int64_t microseconds = 0;
};

// Raw packet data returned by the capture backend.
struct OfflinePacket {
    PacketTimestamp timestamp;
    LinkType linkType = LinkType::Ethernet;
    std::uint32_t capturedLength = 0;
    std::uint32_t originalLength = 0;
    std::vector<std::uint8_t> bytes;
};

// Readers should fill only one of packets or error.
struct PcapReadResult {
    std::vector<OfflinePacket> packets;
    std::optional<std::string> error;
};

struct LiveCaptureOptions {
    std::optional<int> durationSeconds;
    std::optional<int> idleTimeoutSeconds;
    std::function<bool()> stopRequested;
};

class PacketCaptureBackend {
public:
    // Reports whether this build has a usable linked libpcap backend.
    bool pcapAvailable() const;
    std::string backendName() const;

    // Reads the full PCAP file; errors are returned instead of thrown.
    PcapReadResult readPcapFile(
        const std::string& path,
        std::optional<std::string> packetFilter = std::nullopt) const;

    using LiveCaptureCallback = std::function<void(const OfflinePacket&)>;

    // Streams packets directly from an interface.
    std::optional<std::string> captureLive(
        const std::string& interfaceName,
        const LiveCaptureOptions& options,
        std::optional<std::string> packetFilter,
        LiveCaptureCallback callback) const;
};

std::string linkTypeName(LinkType linkType);

} // namespace asset_discovery::capture
