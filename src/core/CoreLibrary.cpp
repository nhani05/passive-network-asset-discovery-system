#include "pnad/core/CoreSession.hpp"

#include "pnad/capture/CapturePacketStream.hpp"
#include "pnad/event/AssetEvent.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>

namespace asset_discovery::core {
namespace {

std::string nowIso8601()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tmUtc {};
    gmtime_r(&time, &tmUtc);
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
    return std::string(buffer);
}

SessionEventSeverity mapSeverity(asset::AssetEventSeverity severity)
{
    switch (severity) {
    case asset::AssetEventSeverity::Info:
        return SessionEventSeverity::Info;
    }
    return SessionEventSeverity::Info;
}

SessionEventType mapAssetEventType(asset::AssetEventType type)
{
    switch (type) {
    case asset::AssetEventType::NewAsset:
        return SessionEventType::AssetCreated;
    }
    return SessionEventType::AssetCreated;
}

std::string formatProgressMessage(const std::string& sourceLabel, const SessionProgress& progress)
{
    std::ostringstream output;
    output << sourceLabel
           << " processed packets=" << progress.packetsParsed
           << " observations=" << progress.observationsProduced
           << " events=" << progress.eventsProduced;
    if (progress.packetsDropped > 0) {
        output << " dropped=" << progress.packetsDropped;
    }
    return output.str();
}

} // namespace

CoreSession::CoreSession(CoreSessionCallbacks callbacks)
    : callbacks_(std::move(callbacks))
{
}

CoreSessionResult CoreSession::analyzePcapFile(
    const std::string& pcapPath,
    CoreSessionOptions options)
{
    publishEvent({
        SessionEventType::CaptureStarted,
        SessionEventSeverity::Info,
        nowIso8601(),
        "PCAP analysis started: " + pcapPath,
        std::nullopt,
        {}
    });

    capture::PacketCaptureBackend backend;
    auto readResult = backend.readPcapFile(pcapPath, options.packetFilter);
    if (readResult.error.has_value()) {
        CoreSessionResult result;
        result.error = readResult.error;
        publishEvent({
            SessionEventType::CaptureError,
            SessionEventSeverity::Error,
            nowIso8601(),
            *readResult.error,
            std::nullopt,
            {}
        });
        return result;
    }

    auto result = processPackets(readResult.packets, std::move(options), "PCAP analysis");
    if (result.error.has_value()) {
        publishEvent({
            SessionEventType::CaptureError,
            SessionEventSeverity::Error,
            nowIso8601(),
            *result.error,
            std::nullopt,
            result.progress
        });
    } else {
        publishEvent({
            SessionEventType::CaptureStopped,
            SessionEventSeverity::Info,
            nowIso8601(),
            "PCAP analysis completed",
            std::nullopt,
            result.progress
        });
    }
    return result;
}

CoreSessionResult CoreSession::processPacketBatch(
    const std::vector<capture::OfflinePacket>& packets,
    CoreSessionOptions options)
{
    publishEvent({
        SessionEventType::CaptureNewData,
        SessionEventSeverity::Info,
        nowIso8601(),
        "Processing packet batch",
        std::nullopt,
        {}
    });
    return processPackets(packets, std::move(options), "Packet batch");
}

CoreSessionResult CoreSession::processPacketStreamBatch(
    std::istream& input,
    std::size_t maxFrames,
    CoreSessionOptions options)
{
    const auto readResult = capture::readPacketStreamFrames(input, maxFrames);
    if (readResult.error.has_value()) {
        CoreSessionResult result;
        result.error = readResult.error;
        publishEvent({
            SessionEventType::CaptureError,
            SessionEventSeverity::Error,
            nowIso8601(),
            *readResult.error,
            std::nullopt,
            {}
        });
        return result;
    }

    publishEvent({
        SessionEventType::CaptureNewData,
        SessionEventSeverity::Info,
        nowIso8601(),
        "Processing tailed packet stream batch",
        std::nullopt,
        {}
    });
    return processPackets(readResult.packets, std::move(options), "Packet stream batch");
}

CoreSessionResult CoreSession::processPackets(
    const std::vector<capture::OfflinePacket>& packets,
    CoreSessionOptions options,
    const std::string& sourceLabel)
{
    options.pipelineOptions.monitorConfig = options.monitorConfig;
    options.pipelineOptions.eventCallback = [this](const asset::AssetEvent& assetEvent) {
        SessionEvent event;
        event.type = mapAssetEventType(assetEvent.type);
        event.severity = mapSeverity(assetEvent.severity);
        event.timestamp = asset::formatEventTimestamp(assetEvent.timestamp);
        event.message = assetEvent.message;
        event.assetEvent = assetEvent;
        publishEvent(std::move(event));
    };
    options.pipelineOptions.assetCallback = [this](const asset::Asset& asset, bool) {
        if (callbacks_.asset) {
            callbacks_.asset(asset);
        }
    };

    auto pipelineResult = live::processPacketsConcurrently(packets, std::move(options.pipelineOptions));
    CoreSessionResult result;
    result.assets = std::move(pipelineResult.assets);
    result.progress = toSessionProgress(pipelineResult.stats);
    result.error = std::move(pipelineResult.error);

    publishProgress(result.progress);
    publishEvent({
        SessionEventType::ProgressUpdated,
        SessionEventSeverity::Info,
        nowIso8601(),
        formatProgressMessage(sourceLabel, result.progress),
        std::nullopt,
        result.progress
    });
    publishSnapshot(result.assets);
    publishAssets(result.assets);

    return result;
}

void CoreSession::publishEvent(SessionEvent event) const
{
    if (callbacks_.event) {
        callbacks_.event(event);
    }
}

void CoreSession::publishProgress(const SessionProgress& progress) const
{
    if (callbacks_.progress) {
        callbacks_.progress(progress);
    }
}

void CoreSession::publishSnapshot(const std::vector<asset::Asset>& assets) const
{
    if (callbacks_.snapshot) {
        callbacks_.snapshot(assets);
    }
}

void CoreSession::publishAssets(const std::vector<asset::Asset>& assets) const
{
    if (!callbacks_.asset) {
        return;
    }
    for (const auto& item : assets) {
        callbacks_.asset(item);
    }
}

std::string sessionEventTypeName(SessionEventType type)
{
    switch (type) {
    case SessionEventType::CaptureStarted:
        return "capture.started";
    case SessionEventType::CaptureNewData:
        return "capture.new_data";
    case SessionEventType::CaptureStopped:
        return "capture.stopped";
    case SessionEventType::CaptureError:
        return "capture.error";
    case SessionEventType::CaptureDropped:
        return "capture.dropped";
    case SessionEventType::AssetCreated:
        return "asset.created";
    case SessionEventType::AssetUpdated:
        return "asset.updated";
    case SessionEventType::EventDetected:
        return "event.detected";
    case SessionEventType::LogMessage:
        return "log.message";
    case SessionEventType::MetricsUpdated:
        return "metrics.updated";
    case SessionEventType::ProgressUpdated:
        return "progress.updated";
    }
    return "log.message";
}

std::string sessionEventSeverityName(SessionEventSeverity severity)
{
    switch (severity) {
    case SessionEventSeverity::Debug:
        return "debug";
    case SessionEventSeverity::Info:
        return "info";
    case SessionEventSeverity::Warning:
        return "warning";
    case SessionEventSeverity::Error:
        return "error";
    }
    return "info";
}

SessionProgress toSessionProgress(const live::LivePipelineStats& stats)
{
    SessionProgress progress;
    progress.packetsRead = stats.packetsCaptured;
    progress.packetsParsed = stats.packetsParsed;
    progress.observationsProduced = stats.observationsProduced;
    progress.observationsApplied = stats.observationsApplied;
    progress.eventsProduced = stats.eventsProduced;
    progress.packetsDropped = stats.packetsDroppedQueueFull + stats.backendPacketsDropped + stats.backendBatchDrops;
    progress.elapsedSeconds = stats.elapsedSeconds;
    return progress;
}

} // namespace asset_discovery::core
