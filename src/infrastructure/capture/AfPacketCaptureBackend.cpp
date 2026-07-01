#include "infrastructure/capture/AfPacketCaptureBackend.hpp"

#include <chrono>
#include <cerrno>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef ASSET_DISCOVERY_HAS_AF_PACKET
#define ASSET_DISCOVERY_HAS_AF_PACKET 0
#endif

#ifndef ASSET_DISCOVERY_HAS_PCAP
#define ASSET_DISCOVERY_HAS_PCAP 0
#endif

#if ASSET_DISCOVERY_HAS_AF_PACKET == 1
#include <arpa/inet.h>
#include <linux/filter.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#if ASSET_DISCOVERY_HAS_PCAP == 1
#include <pcap.h>
#endif
#endif

namespace asset_discovery::capture {
namespace {

#if ASSET_DISCOVERY_HAS_AF_PACKET == 1
using Clock = std::chrono::steady_clock;

constexpr int pollTimeoutMilliseconds = 250;
constexpr std::uint32_t defaultBlockSize = 1U << 20U;
constexpr std::uint32_t defaultFrameSize = 2048U;
constexpr std::uint32_t defaultBlockCount = 64U;
constexpr std::uint32_t defaultBlockRetireTimeoutMilliseconds = 100U;

std::string errnoMessage(const std::string& prefix)
{
    std::ostringstream output;
    output << prefix << ": " << std::strerror(errno);
    return output.str();
}

class FdHandle {
public:
    explicit FdHandle(int fd = -1)
        : fd_(fd)
    {
    }

    ~FdHandle()
    {
        reset();
    }

    FdHandle(const FdHandle&) = delete;
    FdHandle& operator=(const FdHandle&) = delete;

    FdHandle(FdHandle&& other) noexcept
        : fd_(other.fd_)
    {
        other.fd_ = -1;
    }

    FdHandle& operator=(FdHandle&& other) noexcept
    {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const
    {
        return fd_;
    }

    int release()
    {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

    void reset(int fd = -1)
    {
        if (fd_ >= 0) {
            close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_ = -1;
};

struct RingMapping {
    int fd = -1;
    void* memory = nullptr;
    std::size_t length = 0;
    std::uint32_t blockSize = 0;
    std::uint32_t blockCount = 0;

    ~RingMapping()
    {
        if (fd >= 0) {
            tpacket_req3 request = {};
            setsockopt(fd, SOL_PACKET, PACKET_RX_RING, &request, sizeof(request));
        }
        if (memory != nullptr && memory != MAP_FAILED) {
            munmap(memory, length);
        }
        if (fd >= 0) {
            close(fd);
        }
    }

    tpacket_block_desc* block(std::uint32_t index) const
    {
        auto* base = static_cast<std::uint8_t*>(memory);
        return reinterpret_cast<tpacket_block_desc*>(base + (static_cast<std::size_t>(index) * blockSize));
    }
};

struct RingBlockLease {
    std::shared_ptr<RingMapping> ring;
    tpacket_block_desc* block = nullptr;

    ~RingBlockLease()
    {
        if (block != nullptr) {
            __sync_synchronize();
            block->hdr.bh1.block_status = TP_STATUS_KERNEL;
        }
    }
};

std::optional<std::string> attachPromiscuousMembership(int fd, unsigned int ifindex)
{
    packet_mreq membership = {};
    membership.mr_ifindex = static_cast<int>(ifindex);
    membership.mr_type = PACKET_MR_PROMISC;
    if (setsockopt(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &membership, sizeof(membership)) != 0) {
        return errnoMessage("could not enable promiscuous AF_PACKET membership");
    }
    return std::nullopt;
}

std::optional<std::string> attachPacketFilter(
    int fd,
    const std::optional<std::string>& packetFilter,
    const std::string& sourceName)
{
    if (!packetFilter.has_value()) {
        return std::nullopt;
    }

#if ASSET_DISCOVERY_HAS_PCAP == 1
    pcap_t* deadHandle = pcap_open_dead(DLT_EN10MB, 65535);
    if (deadHandle == nullptr) {
        return "could not create libpcap compiler handle for AF_PACKET filter";
    }

    bpf_program program = {};
    if (pcap_compile(deadHandle, &program, packetFilter->c_str(), 1, PCAP_NETMASK_UNKNOWN) != 0) {
        std::ostringstream output;
        output << "invalid BPF filter for '" << sourceName << "': " << pcap_geterr(deadHandle);
        pcap_close(deadHandle);
        return output.str();
    }

    std::vector<sock_filter> instructions;
    instructions.reserve(program.bf_len);
    for (unsigned int i = 0; i < program.bf_len; ++i) {
        const auto& instruction = program.bf_insns[i];
        instructions.push_back({
            instruction.code,
            instruction.jt,
            instruction.jf,
            instruction.k,
        });
    }

    sock_fprog socketFilter = {};
    socketFilter.len = static_cast<unsigned short>(instructions.size());
    socketFilter.filter = instructions.data();
    if (setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &socketFilter, sizeof(socketFilter)) != 0) {
        const auto error = errnoMessage("could not attach BPF filter for '" + sourceName + "'");
        pcap_freecode(&program);
        pcap_close(deadHandle);
        return error;
    }

    pcap_freecode(&program);
    pcap_close(deadHandle);
    return std::nullopt;
#else
    (void)fd;
    (void)sourceName;
    return "cannot apply BPF filter with AF_PACKET: libpcap is not available in this build";
#endif
}

std::optional<std::string> setupRing(int fd, std::shared_ptr<RingMapping>& ring)
{
    tpacket_req3 request = {};
    request.tp_block_size = defaultBlockSize;
    request.tp_frame_size = defaultFrameSize;
    request.tp_block_nr = defaultBlockCount;
    request.tp_frame_nr = (request.tp_block_size / request.tp_frame_size) * request.tp_block_nr;
    request.tp_retire_blk_tov = defaultBlockRetireTimeoutMilliseconds;

    int version = TPACKET_V3;
    if (setsockopt(fd, SOL_PACKET, PACKET_VERSION, &version, sizeof(version)) != 0) {
        return errnoMessage("could not enable TPACKET_V3");
    }

    if (setsockopt(fd, SOL_PACKET, PACKET_RX_RING, &request, sizeof(request)) != 0) {
        return errnoMessage("could not configure AF_PACKET RX ring");
    }

    const auto ringLength = static_cast<std::size_t>(request.tp_block_size) * request.tp_block_nr;
    void* memory = mmap(nullptr, ringLength, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (memory == MAP_FAILED) {
        return errnoMessage("could not mmap AF_PACKET RX ring");
    }

    ring = std::make_shared<RingMapping>();
    ring->fd = fd;
    ring->memory = memory;
    ring->length = ringLength;
    ring->blockSize = request.tp_block_size;
    ring->blockCount = request.tp_block_nr;
    return std::nullopt;
}

void fillPacketStatistics(int fd, BackendStats* stats)
{
    if (stats == nullptr) {
        return;
    }

    tpacket_stats_v3 packetStats = {};
    socklen_t length = sizeof(packetStats);
    if (getsockopt(fd, SOL_PACKET, PACKET_STATISTICS, &packetStats, &length) == 0) {
        stats->available = true;
        stats->packetsReceived = static_cast<std::uint64_t>(packetStats.tp_packets);
        stats->packetsDropped = static_cast<std::uint64_t>(packetStats.tp_drops);
        stats->kernelDrops = static_cast<std::uint64_t>(packetStats.tp_drops);
    }
}

bool shouldStop(
    const LiveCaptureOptions& options,
    Clock::time_point startTime,
    Clock::time_point lastAcceptedPacketTime)
{
    if (options.stopRequested && options.stopRequested()) {
        return true;
    }

    const auto now = Clock::now();
    if (options.durationSeconds.has_value()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        if (elapsed >= *options.durationSeconds) {
            return true;
        }
    }

    if (options.idleTimeoutSeconds.has_value()) {
        const auto idleElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastAcceptedPacketTime).count();
        if (idleElapsed >= *options.idleTimeoutSeconds) {
            return true;
        }
    }

    return false;
}
#endif

} // namespace

std::string AfPacketCaptureBackend::backendName() const
{
    return "af-packet";
}

BackendAvailability AfPacketCaptureBackend::availability() const
{
#if ASSET_DISCOVERY_HAS_AF_PACKET == 1
    FdHandle fd(socket(AF_PACKET, SOCK_RAW | SOCK_CLOEXEC, htons(ETH_P_ALL)));
    if (fd.get() < 0) {
        if (errno == EPERM || errno == EACCES) {
            return {false, "AF_PACKET requires root or CAP_NET_RAW"};
        }
        return {false, errnoMessage("could not open AF_PACKET socket")};
    }
    return {true, {}};
#else
    return {false, "AF_PACKET backend is only supported on Linux builds"};
#endif
}

bool AfPacketCaptureBackend::supportsOfflinePcap() const
{
    return false;
}

PcapReadResult AfPacketCaptureBackend::readPcapFile(
    const std::string& path,
    std::optional<std::string> packetFilter) const
{
    (void)packetFilter;
    return {{}, "cannot read PCAP file '" + path + "': af-packet supports live capture only"};
}

std::optional<std::string> AfPacketCaptureBackend::captureLive(
    const CaptureConfig& config,
    LiveCaptureCallback callback,
    BackendStats* stats) const
{
#if ASSET_DISCOVERY_HAS_AF_PACKET == 1
    if (stats != nullptr) {
        stats->available = true;
        if (stats->selectedBackend.empty()) {
            stats->selectedBackend = backendName();
        }
    }

    FdHandle fd(socket(AF_PACKET, SOCK_RAW | SOCK_CLOEXEC, htons(ETH_P_ALL)));
    if (fd.get() < 0) {
        if (errno == EPERM || errno == EACCES) {
            return "could not open AF_PACKET socket: root or CAP_NET_RAW is required";
        }
        return errnoMessage("could not open AF_PACKET socket");
    }

    const unsigned int ifindex = if_nametoindex(config.interfaceName.c_str());
    if (ifindex == 0) {
        return errnoMessage("could not resolve interface '" + config.interfaceName + "'");
    }

    sockaddr_ll address = {};
    address.sll_family = AF_PACKET;
    address.sll_protocol = htons(ETH_P_ALL);
    address.sll_ifindex = static_cast<int>(ifindex);
    if (bind(fd.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        return errnoMessage("could not bind AF_PACKET socket to '" + config.interfaceName + "'");
    }

    const auto filterError = attachPacketFilter(fd.get(), config.packetFilter, config.interfaceName);
    if (filterError.has_value()) {
        return filterError;
    }

    const auto membershipError = attachPromiscuousMembership(fd.get(), ifindex);
    if (membershipError.has_value()) {
        return membershipError;
    }

    std::shared_ptr<RingMapping> ring;
    const auto ringError = setupRing(fd.get(), ring);
    if (ringError.has_value()) {
        return ringError;
    }
    fd.release();

    const auto startTime = Clock::now();
    auto lastAcceptedPacketTime = startTime;
    std::uint32_t blockIndex = 0;

    while (!shouldStop(config.liveOptions, startTime, lastAcceptedPacketTime)) {
        auto* block = ring->block(blockIndex);
        if ((block->hdr.bh1.block_status & TP_STATUS_USER) == 0) {
            pollfd descriptor = {};
            descriptor.fd = ring->fd;
            descriptor.events = POLLIN;
            const int pollResult = poll(&descriptor, 1, pollTimeoutMilliseconds);
            if (pollResult < 0 && errno != EINTR) {
                fillPacketStatistics(ring->fd, stats);
                return errnoMessage("AF_PACKET poll failed");
            }
            continue;
        }

        auto lease = std::make_shared<RingBlockLease>();
        lease->ring = ring;
        lease->block = block;

        const auto packetCount = block->hdr.bh1.num_pkts;
        std::uint32_t packetOffset = block->hdr.bh1.offset_to_first_pkt;
        for (std::uint32_t packetIndex = 0; packetIndex < packetCount && packetOffset != 0; ++packetIndex) {
            auto* header = reinterpret_cast<tpacket3_hdr*>(
                reinterpret_cast<std::uint8_t*>(block) + packetOffset);
            const auto* data = reinterpret_cast<const std::uint8_t*>(header) + header->tp_mac;

            PacketView packet;
            packet.timestamp.seconds = static_cast<std::int64_t>(header->tp_sec);
            packet.timestamp.microseconds = static_cast<std::int64_t>(header->tp_nsec / 1000U);
            packet.linkType = LinkType::Ethernet;
            packet.capturedLength = header->tp_snaplen;
            packet.originalLength = header->tp_len;
            packet.bytes = {data, header->tp_snaplen};
            packet.owner = lease;

            lastAcceptedPacketTime = Clock::now();
            callback(packet);

            if (header->tp_next_offset == 0) {
                break;
            }
            packetOffset += header->tp_next_offset;
        }

        blockIndex = (blockIndex + 1U) % ring->blockCount;
    }

    fillPacketStatistics(ring->fd, stats);
    return std::nullopt;
#else
    (void)config;
    (void)callback;
    (void)stats;
    return "cannot run live capture: AF_PACKET backend is only supported on Linux builds";
#endif
}

} // namespace asset_discovery::capture
