#include "pnad/app/LiveCapturePipeline.hpp"

#include "pnad/system/BoundedQueue.hpp"
#include "pnad/packet/PacketParserFacade.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>

namespace asset_discovery::live {
namespace {

using Clock = std::chrono::steady_clock;

struct AtomicPipelineStats {
    std::atomic<std::uint64_t> packetsCaptured{0};
    std::atomic<std::uint64_t> packetsEnqueued{0};
    std::atomic<std::uint64_t> packetsDroppedQueueFull{0};
    std::atomic<std::uint64_t> packetsParsed{0};
    std::atomic<std::uint64_t> observationsProduced{0};
    std::atomic<std::uint64_t> observationsApplied{0};
    std::atomic<std::uint64_t> eventsProduced{0};
    std::atomic<std::uint64_t> eventsEnqueued{0};
    std::atomic<std::uint64_t> eventsDroppedQueueFull{0};
    std::atomic<std::uint64_t> packetBatchesDropped{0};
};

parser::ObservationTimestamp toObservationTimestamp(const capture::PacketTimestamp& timestamp)
{
    return {timestamp.seconds, timestamp.microseconds};
}

std::size_t defaultWorkerCount()
{
    const auto hardwareThreads = std::thread::hardware_concurrency();
    if (hardwareThreads <= 2) {
        return 1;
    }
    return static_cast<std::size_t>(hardwareThreads - 1);
}

std::string firstError(const std::optional<std::string>& left, const std::optional<std::string>& right)
{
    if (left.has_value()) {
        return *left;
    }
    if (right.has_value()) {
        return *right;
    }
    return {};
}

class PipelineRunner {
public:
    explicit PipelineRunner(
        LivePipelineOptions options)
        : options_(normalizeLivePipelineOptions(options)),
          packetQueue_(options_.packetQueueCapacity),
          observationQueue_(options_.observationQueueCapacity),
          eventQueue_(options_.eventQueueCapacity),
          assetMonitor_(
              options_.monitorConfig,
              options_.eventCallback
                  ? monitor::AssetMonitor::EventCallback([this](const asset::AssetEvent& event) {
                        enqueueEvent(event);
                    })
                  : monitor::AssetMonitor::EventCallback{})
    {
    }

    template <typename Producer>
    LivePipelineResult run(Producer producer)
    {
        const auto startTime = Clock::now();
        startEventWriter();
        startAggregator();
        startParserWorkers();

        const auto producerError = producer([this](const capture::PacketView& packet) {
            enqueuePacket(packet);
        });
        flushPacketBatch();
        packetQueue_.close();

        for (auto& worker : parserWorkers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        observationQueue_.close();
        if (aggregatorThread_.joinable()) {
            aggregatorThread_.join();
        }
        eventQueue_.close();
        if (eventWriterThread_.joinable()) {
            eventWriterThread_.join();
        }

        const auto endTime = Clock::now();
        auto stats = snapshotStats();
        stats.elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

        LivePipelineResult result;
        result.assets = assetMonitor_.assets();
        result.stats = stats;

        const auto workerError = error();
        if (producerError.has_value() || workerError.has_value()) {
            result.error = firstError(producerError, workerError);
        }
        return result;
    }

    void setBackendStats(const capture::BackendStats& stats)
    {
        backendStats_ = stats;
    }

private:
    void startAggregator()
    {
        aggregatorThread_ = std::thread([this] {
            try {
                ObservationBatch batch;
                while (observationQueue_.waitPop(batch)) {
                    for (const auto& observation : batch.observations) {
                        assetMonitor_.applyObservation(observation);
                    }
                    counters_.observationsApplied.fetch_add(
                        static_cast<std::uint64_t>(batch.observations.size()),
                        std::memory_order_relaxed);
                }
            } catch (const std::exception& error) {
                setError(std::string("asset aggregator failed: ") + error.what());
                packetQueue_.close();
                observationQueue_.close();
            } catch (...) {
                setError("asset aggregator failed with an unknown error");
                packetQueue_.close();
                observationQueue_.close();
            }
        });
    }

    void startEventWriter()
    {
        if (!options_.eventCallback) {
            return;
        }

        eventWriterThread_ = std::thread([this] {
            try {
                asset::AssetEvent event;
                while (eventQueue_.waitPop(event)) {
                    options_.eventCallback(event);
                }
                if (options_.eventFlushCallback) {
                    options_.eventFlushCallback();
                }
            } catch (const std::exception& error) {
                setError(std::string("event writer failed: ") + error.what());
                packetQueue_.close();
                observationQueue_.close();
            } catch (...) {
                setError("event writer failed with an unknown error");
                packetQueue_.close();
                observationQueue_.close();
            }
        });
    }

    void startParserWorkers()
    {
        parserWorkers_.reserve(options_.parserWorkerCount);
        for (std::size_t i = 0; i < options_.parserWorkerCount; ++i) {
            parserWorkers_.emplace_back([this] {
                parserWorkerLoop();
            });
        }
    }

    void parserWorkerLoop()
    {
        try {
            PacketBatch packetBatch;
            while (packetQueue_.waitPop(packetBatch)) {
                ObservationBatch observationBatch;
                for (const auto& packet : packetBatch.packets) {
                    counters_.packetsParsed.fetch_add(1, std::memory_order_relaxed);
                    if (packet.linkType != capture::LinkType::Ethernet) {
                        continue;
                    }

                    auto observations = parser::parseEthernetObservations(
                        packet.bytes,
                        toObservationTimestamp(packet.timestamp));
                    counters_.observationsProduced.fetch_add(
                        static_cast<std::uint64_t>(observations.size()),
                        std::memory_order_relaxed);
                    observationBatch.observations.insert(
                        observationBatch.observations.end(),
                        std::make_move_iterator(observations.begin()),
                        std::make_move_iterator(observations.end()));
                }

                if (!observationBatch.observations.empty()
                    && !observationQueue_.waitPush(std::move(observationBatch))) {
                    setError("observation queue closed before parser workers finished");
                    packetQueue_.close();
                    return;
                }
            }
        } catch (const std::exception& error) {
            setError(std::string("parser worker failed: ") + error.what());
            packetQueue_.close();
            observationQueue_.close();
        } catch (...) {
            setError("parser worker failed with an unknown error");
            packetQueue_.close();
            observationQueue_.close();
        }
    }

    void enqueuePacket(const capture::PacketView& packet)
    {
        counters_.packetsCaptured.fetch_add(1, std::memory_order_relaxed);
        pendingPacketBatch_.packets.push_back(packet);
        if (pendingPacketBatch_.packets.size() >= options_.packetBatchSize) {
            flushPacketBatch();
        }
    }

    void flushPacketBatch()
    {
        if (pendingPacketBatch_.packets.empty()) {
            return;
        }

        const auto packetCount = pendingPacketBatch_.packets.size();
        const auto result = packetQueue_.tryPush(std::move(pendingPacketBatch_));
        if (result == QueuePushResult::Pushed) {
            counters_.packetsEnqueued.fetch_add(
                static_cast<std::uint64_t>(packetCount),
                std::memory_order_relaxed);
        } else {
            counters_.packetsDroppedQueueFull.fetch_add(
                static_cast<std::uint64_t>(packetCount),
                std::memory_order_relaxed);
            counters_.packetBatchesDropped.fetch_add(1, std::memory_order_relaxed);
        }

        pendingPacketBatch_ = PacketBatch{};
        pendingPacketBatch_.packets.reserve(options_.packetBatchSize);
    }

    void enqueueEvent(const asset::AssetEvent& event)
    {
        counters_.eventsProduced.fetch_add(1, std::memory_order_relaxed);
        const auto result = eventQueue_.tryPush(event);
        if (result == QueuePushResult::Pushed) {
            counters_.eventsEnqueued.fetch_add(1, std::memory_order_relaxed);
        } else if (result == QueuePushResult::Full) {
            counters_.eventsDroppedQueueFull.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void setError(std::string message)
    {
        std::lock_guard<std::mutex> lock(errorMutex_);
        if (!error_.has_value()) {
            error_ = std::move(message);
        }
    }

    std::optional<std::string> error() const
    {
        std::lock_guard<std::mutex> lock(errorMutex_);
        return error_;
    }

    LivePipelineStats snapshotStats() const
    {
        LivePipelineStats stats;
        stats.packetsCaptured = counters_.packetsCaptured.load(std::memory_order_relaxed);
        stats.packetsEnqueued = counters_.packetsEnqueued.load(std::memory_order_relaxed);
        stats.packetsDroppedQueueFull = counters_.packetsDroppedQueueFull.load(std::memory_order_relaxed);
        stats.packetsParsed = counters_.packetsParsed.load(std::memory_order_relaxed);
        stats.observationsProduced = counters_.observationsProduced.load(std::memory_order_relaxed);
        stats.observationsApplied = counters_.observationsApplied.load(std::memory_order_relaxed);
        stats.eventsProduced = counters_.eventsProduced.load(std::memory_order_relaxed);
        stats.eventsEnqueued = counters_.eventsEnqueued.load(std::memory_order_relaxed);
        stats.eventsDroppedQueueFull = counters_.eventsDroppedQueueFull.load(std::memory_order_relaxed);
        stats.packetQueueHighWatermark = packetQueue_.highWatermark();
        stats.observationQueueHighWatermark = observationQueue_.highWatermark();
        stats.eventQueueHighWatermark = eventQueue_.highWatermark();
        stats.packetBatchSize = options_.packetBatchSize;
        stats.packetQueueCapacity = options_.packetQueueCapacity;
        stats.observationQueueCapacity = options_.observationQueueCapacity;
        stats.eventQueueCapacity = options_.eventQueueCapacity;
        stats.parserWorkerCount = options_.parserWorkerCount;
        stats.backendStatsAvailable = backendStats_.available;
        stats.backendRequested = backendStats_.requestedBackend;
        stats.backendSelected = backendStats_.selectedBackend;
        stats.backendFallbackReason = backendStats_.fallbackReason;
        stats.backendPacketsReceived = backendStats_.packetsReceived;
        stats.backendPacketsDropped = backendStats_.packetsDropped;
        stats.backendPacketsInterfaceDropped = backendStats_.packetsInterfaceDropped;
        stats.backendPacketsCopied = backendStats_.packetsCopied;
        stats.backendKernelDrops = backendStats_.kernelDrops;
        stats.packetBatchesDropped = counters_.packetBatchesDropped.load(std::memory_order_relaxed);
        stats.backendBatchDrops = backendStats_.batchDrops;
        return stats;
    }

    LivePipelineOptions options_;
    BoundedQueue<PacketBatch> packetQueue_;
    BoundedQueue<ObservationBatch> observationQueue_;
    BoundedQueue<asset::AssetEvent> eventQueue_;
    AtomicPipelineStats counters_;
    capture::BackendStats backendStats_;
    PacketBatch pendingPacketBatch_;
    std::vector<std::thread> parserWorkers_;
    std::thread aggregatorThread_;
    std::thread eventWriterThread_;
    monitor::AssetMonitor assetMonitor_;
    mutable std::mutex errorMutex_;
    std::optional<std::string> error_;
};

} // namespace

LivePipelineOptions normalizeLivePipelineOptions(LivePipelineOptions options)
{
    if (options.packetBatchSize == 0) {
        options.packetBatchSize = 128;
    }
    if (options.observationQueueCapacity == 0) {
        options.observationQueueCapacity = 1024;
    }
    if (options.eventQueueCapacity == 0) {
        options.eventQueueCapacity = 1024;
    }
    if (options.parserWorkerCount == 0) {
        options.parserWorkerCount = defaultWorkerCount();
    }
    return options;
}

LivePipelineResult runLiveCapturePipeline(
    const capture::CaptureBackend& backend,
    capture::CaptureConfig captureConfig,
    capture::BackendStats initialBackendStats,
    LivePipelineOptions options)
{
    PipelineRunner runner(options);
    capture::BackendStats backendStats = std::move(initialBackendStats);
    backendStats.selectedBackend = backend.backendName();
    runner.setBackendStats(backendStats);

    auto result = runner.run([&](const auto& enqueuePacket) -> std::optional<std::string> {
        std::optional<std::string> captureError;
        std::thread captureThread([&] {
            captureError = backend.captureLive(
                captureConfig,
                [&](const capture::PacketView& packet) {
                    enqueuePacket(packet);
                },
                &backendStats);
        });
        if (captureThread.joinable()) {
            captureThread.join();
        }
        runner.setBackendStats(backendStats);
        return captureError;
    });
    return result;
}

LivePipelineResult processPacketsConcurrently(
    const std::vector<capture::OfflinePacket>& packets,
    LivePipelineOptions options)
{
    PipelineRunner runner(options);
    return runner.run([&](const auto& enqueuePacket) -> std::optional<std::string> {
        for (const auto& packet : packets) {
            enqueuePacket(packet.view());
        }
        return std::nullopt;
    });
}

std::string formatLivePipelineMetrics(const LivePipelineStats& stats)
{
    std::ostringstream output;
    const auto throughput = stats.elapsedSeconds > 0.0
        ? static_cast<double>(stats.packetsParsed) / stats.elapsedSeconds
        : 0.0;

    output << "live_capture_metrics"
           << " elapsed_seconds=" << std::fixed << std::setprecision(6) << stats.elapsedSeconds
           << " packets_captured=" << stats.packetsCaptured
           << " packets_enqueued=" << stats.packetsEnqueued
           << " packets_dropped_queue_full=" << stats.packetsDroppedQueueFull
           << " packets_parsed=" << stats.packetsParsed
           << " packet_throughput_per_second=" << std::fixed << std::setprecision(2) << throughput
           << " observations_produced=" << stats.observationsProduced
           << " observations_applied=" << stats.observationsApplied
           << " events_produced=" << stats.eventsProduced
           << " events_enqueued=" << stats.eventsEnqueued
           << " events_dropped_queue_full=" << stats.eventsDroppedQueueFull
           << " packet_queue_high_watermark=" << stats.packetQueueHighWatermark
           << " observation_queue_high_watermark=" << stats.observationQueueHighWatermark
           << " event_queue_high_watermark=" << stats.eventQueueHighWatermark
           << " packet_batch_size=" << stats.packetBatchSize
           << " packet_queue_capacity=" << stats.packetQueueCapacity
           << " observation_queue_capacity=" << stats.observationQueueCapacity
           << " event_queue_capacity=" << stats.eventQueueCapacity
           << " parser_worker_count=" << stats.parserWorkerCount
           << " packet_batches_dropped=" << stats.packetBatchesDropped;

    if (!stats.backendRequested.empty()) {
        output << " backend_requested=" << stats.backendRequested;
    }
    if (!stats.backendSelected.empty()) {
        output << " backend_selected=" << stats.backendSelected;
    }
    if (!stats.backendFallbackReason.empty()) {
        output << " backend_fallback_reason=\"" << stats.backendFallbackReason << '"';
    }

    if (stats.backendStatsAvailable || !stats.backendSelected.empty()) {
        output << " backend_packets_received=" << stats.backendPacketsReceived
               << " backend_packets_dropped=" << stats.backendPacketsDropped
               << " backend_packets_interface_dropped=" << stats.backendPacketsInterfaceDropped
               << " backend_packets_copied=" << stats.backendPacketsCopied
               << " backend_kernel_drops=" << stats.backendKernelDrops
               << " backend_batch_drops=" << stats.backendBatchDrops;
    }

    output << '\n';
    return output.str();
}

} // namespace asset_discovery::live
