#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::capture {

enum class LinkType {
    Ethernet,
};

// Timestamp copied from the packet source without timezone conversion.
struct PacketTimestamp {
    std::int64_t seconds = 0;
    std::int64_t microseconds = 0;
};

// Raw packet data as reported by the capture backend.
struct OfflinePacket {
    PacketTimestamp timestamp;
    LinkType linkType = LinkType::Ethernet;
    std::uint32_t capturedLength = 0;
    std::uint32_t originalLength = 0;
    std::vector<std::uint8_t> bytes;
};

// Exactly one of packets or error is expected to be populated by readers.
struct PcapReadResult {
    std::vector<OfflinePacket> packets;
    std::optional<std::string> error;
};

class PacketCaptureBackend {
public:
    // Report whether this build was linked with a usable libpcap backend.
    bool pcapAvailable() const;
    std::string backendName() const;

    // Read a PCAP file completely; errors are returned instead of thrown.
    PcapReadResult readPcapFile(const std::string& path) const;
};

std::string linkTypeName(LinkType linkType);

} // namespace asset_discovery::capture
