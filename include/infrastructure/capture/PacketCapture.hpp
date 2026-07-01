#pragma once

#include "domain/ByteView.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::capture {

enum class LinkType {
    Ethernet,
};

enum class CaptureBackendSelection {
    Auto,
    Pcap,
    AfPacket,
};

// Timestamp is preserved from the packet source without timezone conversion.
struct PacketTimestamp {
    std::int64_t seconds = 0;
    std::int64_t microseconds = 0;
};

struct PacketView {
    PacketTimestamp timestamp;
    LinkType linkType = LinkType::Ethernet;
    std::uint32_t capturedLength = 0;
    std::uint32_t originalLength = 0;
    ByteView bytes;
    std::shared_ptr<const void> owner;
};

// Raw packet data returned by the capture backend.
struct OfflinePacket {
    PacketTimestamp timestamp;
    LinkType linkType = LinkType::Ethernet;
    std::uint32_t capturedLength = 0;
    std::uint32_t originalLength = 0;
    std::vector<std::uint8_t> bytes;

    PacketView view() const;
};

// Readers should fill only one of packets or error.
struct PcapReadResult {
    std::vector<OfflinePacket> packets;
    std::optional<std::string> error;
};

struct LiveCaptureBackendStats {
    bool available = false;
    std::string requestedBackend;
    std::string selectedBackend;
    std::string fallbackReason;
    std::uint64_t packetsReceived = 0;
    std::uint64_t packetsDropped = 0;
    std::uint64_t packetsInterfaceDropped = 0;
    std::uint64_t packetsCopied = 0;
    std::uint64_t kernelDrops = 0;
    std::uint64_t queueDrops = 0;
    std::uint64_t batchDrops = 0;
};

using BackendStats = LiveCaptureBackendStats;

struct BackendAvailability {
    bool available = false;
    std::string reason;
};

struct LiveCaptureOptions {
    std::optional<int> durationSeconds;
    std::optional<int> idleTimeoutSeconds;
    std::function<bool()> stopRequested;
};

struct CaptureConfig {
    std::string interfaceName;
    LiveCaptureOptions liveOptions;
    std::optional<std::string> packetFilter;
    CaptureBackendSelection requestedBackend = CaptureBackendSelection::Auto;
};

class CaptureBackend {
public:
    using LiveCaptureCallback = std::function<void(const PacketView&)>;

    virtual ~CaptureBackend() = default;
    virtual std::string backendName() const = 0;
    virtual BackendAvailability availability() const = 0;
    virtual bool supportsOfflinePcap() const = 0;

    virtual PcapReadResult readPcapFile(
        const std::string& path,
        std::optional<std::string> packetFilter = std::nullopt) const = 0;

    virtual std::optional<std::string> captureLive(
        const CaptureConfig& config,
        LiveCaptureCallback callback,
        BackendStats* stats = nullptr) const = 0;
};

class PcapCaptureBackend : public CaptureBackend {
public:
    // Reports whether this build has a usable linked libpcap backend.
    bool pcapAvailable() const;
    std::string backendName() const override;
    BackendAvailability availability() const override;
    bool supportsOfflinePcap() const override;

    // Reads the full PCAP file; errors are returned instead of thrown.
    PcapReadResult readPcapFile(
        const std::string& path,
        std::optional<std::string> packetFilter = std::nullopt) const override;

    // Streams packets directly from an interface.
    std::optional<std::string> captureLive(
        const CaptureConfig& config,
        LiveCaptureCallback callback,
        BackendStats* stats = nullptr) const override;
};

using PacketCaptureBackend = PcapCaptureBackend;

struct CaptureBackendFactoryResult {
    std::unique_ptr<CaptureBackend> backend;
    BackendStats initialStats;
    std::optional<std::string> error;
};

CaptureBackendFactoryResult createCaptureBackend(CaptureBackendSelection requested);

std::string linkTypeName(LinkType linkType);
std::string captureBackendSelectionName(CaptureBackendSelection selection);
std::optional<CaptureBackendSelection> parseCaptureBackendSelection(const std::string& value);

} // namespace asset_discovery::capture
