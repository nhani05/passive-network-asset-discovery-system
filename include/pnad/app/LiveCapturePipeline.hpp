#pragma once

#include "pnad/discovery/AssetMonitor.hpp"
#include "pnad/event/AssetEvent.hpp"
#include "pnad/discovery/AssetObservation.hpp"
#include "pnad/discovery/AssetStore.hpp"
#include "pnad/capture/PacketCapture.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::live {

struct PacketBatch {
    std::vector<capture::PacketView> packets;
};

struct ObservationBatch {
    std::vector<parser::AssetObservation> observations;
};

struct LivePipelineOptions {
    std::size_t packetBatchSize = 128;
    std::size_t packetQueueCapacity = 1024;
    std::size_t observationQueueCapacity = 1024;
    std::size_t eventQueueCapacity = 1024;
    std::size_t parserWorkerCount = 0;
    monitor::AssetMonitorConfig monitorConfig;
    std::function<void(const asset::AssetEvent&)> eventCallback;
    std::function<void()> eventFlushCallback;
};

struct LivePipelineStats {
    std::uint64_t packetsCaptured = 0;
    std::uint64_t packetsEnqueued = 0;
    std::uint64_t packetsDroppedQueueFull = 0;
    std::uint64_t packetsParsed = 0;
    std::uint64_t observationsProduced = 0;
    std::uint64_t observationsApplied = 0;
    std::uint64_t eventsProduced = 0;
    std::uint64_t eventsEnqueued = 0;
    std::uint64_t eventsDroppedQueueFull = 0;
    std::size_t packetQueueHighWatermark = 0;
    std::size_t observationQueueHighWatermark = 0;
    std::size_t eventQueueHighWatermark = 0;
    double elapsedSeconds = 0.0;
    bool backendStatsAvailable = false;
    std::string backendRequested;
    std::string backendSelected;
    std::string backendFallbackReason;
    std::uint64_t backendPacketsReceived = 0;
    std::uint64_t backendPacketsDropped = 0;
    std::uint64_t backendPacketsInterfaceDropped = 0;
    std::uint64_t backendPacketsCopied = 0;
    std::uint64_t backendKernelDrops = 0;
    std::uint64_t packetBatchesDropped = 0;
    std::uint64_t backendBatchDrops = 0;
    std::size_t packetBatchSize = 0;
    std::size_t packetQueueCapacity = 0;
    std::size_t observationQueueCapacity = 0;
    std::size_t eventQueueCapacity = 0;
    std::size_t parserWorkerCount = 0;
};

struct LivePipelineResult {
    std::vector<asset::Asset> assets;
    LivePipelineStats stats;
    std::optional<std::string> error;
};

LivePipelineOptions normalizeLivePipelineOptions(LivePipelineOptions options);

LivePipelineResult runLiveCapturePipeline(
    const capture::CaptureBackend& backend,
    capture::CaptureConfig captureConfig,
    capture::BackendStats initialBackendStats = {},
    LivePipelineOptions options = {});

LivePipelineResult processPacketsConcurrently(
    const std::vector<capture::OfflinePacket>& packets,
    LivePipelineOptions options = {});

std::string formatLivePipelineMetrics(const LivePipelineStats& stats);

} // namespace asset_discovery::live
