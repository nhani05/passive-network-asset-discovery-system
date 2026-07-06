#pragma once

#include "pnad/app/LiveCapturePipeline.hpp"
#include "pnad/capture/PacketCapture.hpp"
#include "pnad/discovery/AssetStore.hpp"
#include "pnad/event/AssetEvent.hpp"

#include <cstdint>
#include <functional>
#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::core {

enum class SessionEventSeverity {
    Debug,
    Info,
    Warning,
    Error,
};

enum class SessionEventType {
    CaptureStarted,
    CaptureNewData,
    CaptureStopped,
    CaptureError,
    CaptureDropped,
    AssetCreated,
    AssetUpdated,
    EventDetected,
    LogMessage,
    MetricsUpdated,
    ProgressUpdated,
};

struct SessionProgress {
    std::uint64_t packetsRead = 0;
    std::uint64_t packetsParsed = 0;
    std::uint64_t observationsProduced = 0;
    std::uint64_t observationsApplied = 0;
    std::uint64_t eventsProduced = 0;
    std::uint64_t packetsDropped = 0;
    double elapsedSeconds = 0.0;
};

struct SessionEvent {
    SessionEventType type = SessionEventType::LogMessage;
    SessionEventSeverity severity = SessionEventSeverity::Info;
    std::string timestamp;
    std::string message;
    std::optional<asset::AssetEvent> assetEvent;
    SessionProgress progress;
};

struct CoreSessionCallbacks {
    std::function<void(const SessionEvent&)> event;
    std::function<void(const asset::Asset&)> asset;
    std::function<void(const std::vector<asset::Asset>&)> snapshot;
    std::function<void(const SessionProgress&)> progress;
};

struct CoreSessionOptions {
    std::optional<std::string> packetFilter;
    monitor::AssetMonitorConfig monitorConfig;
    live::LivePipelineOptions pipelineOptions;
};

struct CoreSessionResult {
    std::vector<asset::Asset> assets;
    SessionProgress progress;
    std::optional<std::string> error;
};

class CoreSession {
public:
    explicit CoreSession(CoreSessionCallbacks callbacks = {});

    CoreSessionResult analyzePcapFile(
        const std::string& pcapPath,
        CoreSessionOptions options = {});

    CoreSessionResult processPacketBatch(
        const std::vector<capture::OfflinePacket>& packets,
        CoreSessionOptions options = {});

    CoreSessionResult processPacketStreamBatch(
        std::istream& input,
        std::size_t maxFrames,
        CoreSessionOptions options = {});

private:
    CoreSessionResult processPackets(
        const std::vector<capture::OfflinePacket>& packets,
        CoreSessionOptions options,
        const std::string& sourceLabel);

    void publishEvent(SessionEvent event) const;
    void publishProgress(const SessionProgress& progress) const;
    void publishSnapshot(const std::vector<asset::Asset>& assets) const;
    void publishAssets(const std::vector<asset::Asset>& assets) const;

    CoreSessionCallbacks callbacks_;
};

std::string sessionEventTypeName(SessionEventType type);
std::string sessionEventSeverityName(SessionEventSeverity severity);
SessionProgress toSessionProgress(const live::LivePipelineStats& stats);

} // namespace asset_discovery::core
