#include "pnad/app/LiveCapturePipeline.hpp"
#include "pnad/system/BoundedQueue.hpp"
#include "pnad/event/AssetEvent.hpp"
#include "pnad/discovery/JsonRenderer.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using asset_discovery::capture::LinkType;
using asset_discovery::capture::OfflinePacket;
using asset_discovery::capture::PacketView;
using asset_discovery::live::BoundedQueue;
using asset_discovery::live::PacketBatch;
using asset_discovery::live::QueuePushResult;
using asset_discovery::live::runLiveCapturePipeline;
using asset_discovery::live::LivePipelineOptions;
using asset_discovery::live::formatLivePipelineMetrics;
using asset_discovery::live::processPacketsConcurrently;
using asset_discovery::output::renderAssetJson;
using asset_discovery::asset::AssetEvent;
using asset_discovery::asset::AssetEventType;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

std::vector<std::uint8_t> macBytes(const std::string& value)
{
    std::vector<std::uint8_t> result;
    std::istringstream input(value);
    std::string part;
    while (std::getline(input, part, ':')) {
        result.push_back(static_cast<std::uint8_t>(std::stoul(part, nullptr, 16)));
    }
    return result;
}

std::vector<std::uint8_t> ipv4Bytes(const std::string& value)
{
    std::vector<std::uint8_t> result;
    std::istringstream input(value);
    std::string part;
    while (std::getline(input, part, '.')) {
        result.push_back(static_cast<std::uint8_t>(std::stoul(part)));
    }
    return result;
}

void appendUInt16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void appendBytes(std::vector<std::uint8_t>& bytes, const std::vector<std::uint8_t>& suffix)
{
    bytes.insert(bytes.end(), suffix.begin(), suffix.end());
}

struct LeaseState {
    std::vector<std::uint8_t> bytes;
    std::shared_ptr<int> releases;

    explicit LeaseState(std::vector<std::uint8_t> value, std::shared_ptr<int> releaseCounter)
        : bytes(std::move(value))
        , releases(std::move(releaseCounter))
    {
    }

    ~LeaseState()
    {
        ++(*releases);
    }
};

OfflinePacket arpPacket(
    const std::string& sourceMac,
    const std::string& sourceIp,
    std::int64_t seconds)
{
    std::vector<std::uint8_t> bytes;
    appendBytes(bytes, macBytes("ff:ff:ff:ff:ff:ff"));
    appendBytes(bytes, macBytes(sourceMac));
    appendUInt16(bytes, 0x0806);
    appendUInt16(bytes, 1);
    appendUInt16(bytes, 0x0800);
    bytes.push_back(6);
    bytes.push_back(4);
    appendUInt16(bytes, 1);
    appendBytes(bytes, macBytes(sourceMac));
    appendBytes(bytes, ipv4Bytes(sourceIp));
    appendBytes(bytes, macBytes("00:00:00:00:00:00"));
    appendBytes(bytes, ipv4Bytes("192.168.1.1"));

    OfflinePacket packet;
    packet.timestamp.seconds = seconds;
    packet.linkType = LinkType::Ethernet;
    packet.capturedLength = static_cast<std::uint32_t>(bytes.size());
    packet.originalLength = static_cast<std::uint32_t>(bytes.size());
    packet.bytes = std::move(bytes);
    return packet;
}

PacketView packetViewWithLease(
    const OfflinePacket& packet,
    const std::shared_ptr<int>& releaseCounter)
{
    auto lease = std::make_shared<LeaseState>(packet.bytes, releaseCounter);
    PacketView view;
    view.timestamp = packet.timestamp;
    view.linkType = packet.linkType;
    view.capturedLength = packet.capturedLength;
    view.originalLength = packet.originalLength;
    view.bytes = asset_discovery::makeByteView(lease->bytes);
    view.owner = lease;
    return view;
}

class FakeCaptureBackend final : public asset_discovery::capture::CaptureBackend {
public:
    explicit FakeCaptureBackend(std::vector<PacketView> packets)
        : packets_(std::move(packets))
    {
    }

    std::string backendName() const override
    {
        return "fake";
    }

    asset_discovery::capture::BackendAvailability availability() const override
    {
        return {true, {}};
    }

    bool supportsOfflinePcap() const override
    {
        return false;
    }

    asset_discovery::capture::PcapReadResult readPcapFile(
        const std::string& path,
        std::optional<std::string> packetFilter = std::nullopt) const override
    {
        (void)packetFilter;
        return {{}, "fake backend cannot read PCAP file '" + path + "'"};
    }

    std::optional<std::string> captureLive(
        const asset_discovery::capture::CaptureConfig& config,
        LiveCaptureCallback callback,
        asset_discovery::capture::BackendStats* stats = nullptr) const override
    {
        (void)config;
        const auto packetCount = packets_.size();
        for (const auto& packet : packets_) {
            callback(packet);
        }
        packets_.clear();
        if (stats != nullptr) {
            stats->available = true;
            stats->selectedBackend = backendName();
            stats->packetsReceived = packetCount;
        }
        return std::nullopt;
    }

private:
    mutable std::vector<PacketView> packets_;
};

void parsesPacketsAndAggregatesAssets()
{
    const std::vector<OfflinePacket> packets = {
        arpPacket("02:42:ac:11:00:02", "192.168.1.10", 10),
        arpPacket("02:42:ac:11:00:03", "192.168.1.11", 11),
    };
    LivePipelineOptions options;
    options.packetBatchSize = 1;
    options.packetQueueCapacity = 4;
    options.observationQueueCapacity = 4;
    options.parserWorkerCount = 2;

    const auto result = processPacketsConcurrently(packets, options);

    expect(!result.error.has_value(), "pipeline should process valid packets without error");
    expect(result.assets.size() == 2, "aggregator should produce one asset per MAC");
    expect(result.stats.packetsCaptured == 2, "stats should count captured packets");
    expect(result.stats.packetsEnqueued == 2, "stats should count enqueued packets");
    expect(result.stats.packetsParsed == 2, "stats should count parsed packets");
    expect(result.stats.observationsProduced == 2, "stats should count produced observations");
    expect(result.stats.observationsApplied == 2, "stats should count applied observations");
    expect(result.stats.parserWorkerCount == 2, "stats should preserve worker count");
}

void keepsMetricsSeparateFromJsonAssets()
{
    const std::vector<OfflinePacket> packets = {
        arpPacket("02:42:ac:11:00:04", "192.168.1.12", 12),
    };
    LivePipelineOptions options;
    options.packetBatchSize = 1;
    options.packetQueueCapacity = 2;
    options.observationQueueCapacity = 2;
    options.parserWorkerCount = 1;

    const auto result = processPacketsConcurrently(packets, options);
    const auto json = renderAssetJson(result.assets);
    const auto metrics = formatLivePipelineMetrics(result.stats);

    expect(json.find("live_capture_metrics") == std::string::npos, "asset JSON should not include metrics");
    expect(metrics.find("live_capture_metrics") != std::string::npos, "metrics should use separate text format");
    expect(metrics.find("packets_parsed=1") != std::string::npos, "metrics should report parsed packets");
}

void keepsBorrowedPacketLeaseUntilWorkersFinish()
{
    auto releases = std::make_shared<int>(0);
    const auto packet = arpPacket("02:42:ac:11:00:05", "192.168.1.13", 13);
    FakeCaptureBackend backend({packetViewWithLease(packet, releases)});

    LivePipelineOptions options;
    options.packetBatchSize = 1;
    options.packetQueueCapacity = 2;
    options.observationQueueCapacity = 2;
    options.parserWorkerCount = 1;

    asset_discovery::capture::CaptureConfig config;
    config.interfaceName = "fake0";
    asset_discovery::capture::BackendStats backendStats;
    backendStats.requestedBackend = "fake";

    const auto result = runLiveCapturePipeline(backend, config, backendStats, options);

    expect(!result.error.has_value(), "fake live backend should process without error");
    expect(result.assets.size() == 1, "borrowed packet should parse before lease is released");
    expect(*releases == 1, "borrowed packet owner should release exactly once after pipeline drains");
    expect(result.stats.backendSelected == "fake", "metrics should retain selected backend");
}

void dispatchesEventsThroughWriterThread()
{
    const std::vector<OfflinePacket> packets = {
        arpPacket("02:42:ac:11:00:07", "192.168.1.15", 15),
        arpPacket("02:42:ac:11:00:08", "192.168.1.16", 16),
    };
    std::atomic<int> newAssetEvents{0};
    std::atomic<bool> flushed{false};

    LivePipelineOptions options;
    options.packetBatchSize = 1;
    options.packetQueueCapacity = 4;
    options.observationQueueCapacity = 4;
    options.eventQueueCapacity = 4;
    options.parserWorkerCount = 1;
    options.eventCallback = [&](const AssetEvent& event) {
        if (event.type == AssetEventType::NewAsset) {
            newAssetEvents.fetch_add(1);
        }
    };
    options.eventFlushCallback = [&]() {
        flushed.store(true);
    };

    const auto result = processPacketsConcurrently(packets, options);

    expect(!result.error.has_value(), "pipeline with event writer should process without error");
    expect(result.assets.size() == 2, "event writer should not change final asset output");
    expect(newAssetEvents.load() == 2, "event callback should receive new asset events");
    expect(flushed.load(), "event writer should flush before result is returned");
    expect(result.stats.eventsProduced == 2, "stats should count produced events");
    expect(result.stats.eventsEnqueued == 2, "stats should count enqueued events");
    expect(result.stats.eventQueueHighWatermark >= 1, "stats should record event queue high watermark");
}

void countsEventQueueDrops()
{
    const std::vector<OfflinePacket> packets = {
        arpPacket("02:42:ac:11:00:09", "192.168.1.17", 17),
        arpPacket("02:42:ac:11:00:0a", "192.168.1.18", 18),
        arpPacket("02:42:ac:11:00:0b", "192.168.1.19", 19),
        arpPacket("02:42:ac:11:00:0c", "192.168.1.20", 20),
    };

    LivePipelineOptions options;
    options.packetBatchSize = 4;
    options.packetQueueCapacity = 4;
    options.observationQueueCapacity = 4;
    options.eventQueueCapacity = 1;
    options.parserWorkerCount = 1;
    options.eventCallback = [](const AssetEvent&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };

    const auto result = processPacketsConcurrently(packets, options);

    expect(!result.error.has_value(), "pipeline should continue when event queue drops");
    expect(result.assets.size() == 4, "event drops should not drop asset observations");
    expect(result.stats.eventsProduced == 4, "stats should count all produced events");
    expect(result.stats.eventsDroppedQueueFull >= 1, "stats should count event queue drops");
}

void releasesBorrowedPacketLeaseWhenBatchDrops()
{
    auto releases = std::make_shared<int>(0);
    const auto packet = arpPacket("02:42:ac:11:00:06", "192.168.1.14", 14);

    BoundedQueue<PacketBatch> queue(0);
    PacketBatch batch;
    batch.packets.push_back(packetViewWithLease(packet, releases));
    const auto result = queue.tryPush(std::move(batch));

    expect(result == QueuePushResult::Full, "zero-capacity queue should reject packet batch");
    expect(*releases == 1, "dropped packet owner should release after failed enqueue");
}

} // namespace

int main()
{
    parsesPacketsAndAggregatesAssets();
    keepsMetricsSeparateFromJsonAssets();
    keepsBorrowedPacketLeaseUntilWorkersFinish();
    dispatchesEventsThroughWriterThread();
    countsEventQueueDrops();
    releasesBorrowedPacketLeaseWhenBatchDrops();

    if (failures != 0) {
        std::cerr << failures << " live capture pipeline test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
