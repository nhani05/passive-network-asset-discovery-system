#include "pnad/backend/CaptureService.hpp"

#include "pnad/app/LiveCapturePipeline.hpp"
#include "pnad/backend/EventBus.hpp"
#include "pnad/backend/RestApi.hpp"
#include "pnad/capture/PacketCapture.hpp"
#include "pnad/constants/CaptureConstants.hpp"
#include "pnad/discovery/AssetMonitor.hpp"
#include "pnad/packet/PacketParserFacade.hpp"
#include "pnad/storage/SQLiteWriter.hpp"

#include <exception>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

namespace asset_discovery::backend {
namespace {

asset_discovery::parser::ObservationTimestamp toObservationTimestamp(
    const asset_discovery::capture::PacketTimestamp& timestamp)
{
    return {timestamp.seconds, timestamp.microseconds};
}

asset_discovery::monitor::AssetMonitorConfig defaultMonitorConfig(
    const std::string& interfaceName)
{
    asset_discovery::monitor::AssetMonitorConfig config;
    config.interfaceName = interfaceName;
    return config;
}

std::string metadataMapToJson(const std::map<std::string, std::string>& metadata)
{
    if (metadata.empty()) {
        return "{}";
    }
    std::ostringstream output;
    output << "{";
    bool first = true;
    for (const auto& pair : metadata) {
        if (!first) {
            output << ",";
        }
        output << asset_discovery::backend::jsonString(pair.first) << ":" << asset_discovery::backend::jsonString(pair.second);
        first = false;
    }
    output << "}";
    return output.str();
}

asset_discovery::backend::BackendEventRecord toBackendEventRecord(const asset_discovery::asset::AssetEvent& event)
{
    asset_discovery::backend::BackendEventRecord record;
    record.eventTime = asset_discovery::asset::formatEventTimestamp(event.timestamp);
    record.eventType = asset_discovery::asset::assetEventTypeName(event.type);
    record.severity = asset_discovery::asset::assetEventSeverityLabel(event.severity);
    record.ipAddress = event.ipAddress.value_or("");
    record.macAddress = event.macAddress.value_or("");
    record.message = event.message;
    record.metadataJson = metadataMapToJson(event.metadata);
    return record;
}

asset_discovery::backend::BackendAssetRecord assetRecordFromEvent(const asset_discovery::asset::AssetEvent& event)
{
    asset_discovery::backend::BackendAssetRecord record;
    record.macAddress = event.macAddress.value_or("");
    if (event.ipAddress.has_value()) {
        record.ipAddressesJson = "[\"" + *event.ipAddress + "\"]";
    } else {
        record.ipAddressesJson = "[]";
    }
    record.hostname = event.hostname.value_or("");
    record.displayName = event.hostname.value_or("");
    record.firstSeen = asset_discovery::asset::formatEventTimestamp(event.timestamp);
    record.lastSeen = asset_discovery::asset::formatEventTimestamp(event.timestamp);
    record.discoverySourcesJson = "[\"" + event.protocol + "\"]";
    return record;
}

void publishAssetEvent(const asset_discovery::asset::AssetEvent& event, const std::string& sqlitePath, std::uint64_t startCount, std::uint64_t stopCount)
{
    using namespace asset_discovery::backend;

    auto record = toBackendEventRecord(event);
    EventBus::rx().publish({
        "event.detected",
        currentIso8601Timestamp(),
        "info",
        EventDetectedEvent{record}
    });

    if (event.type == asset_discovery::asset::AssetEventType::NewAsset) {
        auto assetRec = assetRecordFromEvent(event);
        EventBus::rx().publish({
            "asset.created",
            currentIso8601Timestamp(),
            "info",
            AssetCreatedEvent{assetRec}
        });
    }

    try {
        storage::SQLiteWriter writer(sqlitePath);
        BackendMetricsSnapshot snapshot;
        writer.countAssets(snapshot.assetCount);
        writer.countEvents(snapshot.eventCount);
        snapshot.captureStarts = startCount;
        snapshot.captureStops = stopCount;
        EventBus::rx().publish({
            "metrics.updated",
            currentIso8601Timestamp(),
            "info",
            MetricsUpdatedEvent{snapshot}
        });
    } catch (...) {}
}

void persistAsset(const asset_discovery::asset::Asset& asset, const std::string& sqlitePath)
{
    storage::SQLiteWriter writer(sqlitePath);
    if (const auto error = writer.writeAssets({asset}); error.has_value()) {
        throw std::runtime_error(*error);
    }
}

} // namespace

CaptureService::CaptureService(BackendConfig config)
    : config_(std::move(config))
{
}

CaptureService::~CaptureService()
{
    shutdown();
}

CaptureCommandResult CaptureService::start()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == CaptureServiceState::Running || state_ == CaptureServiceState::Starting) {
            CaptureCommandResult result;
            result.status = statusLocked();
            result.error = "capture is already running";
            return result;
        }
        if (config_.captureMode == BackendCaptureMode::Live && !config_.interfaceName.has_value()) {
            CaptureCommandResult result;
            result.status = statusLocked();
            result.error = "live capture requires --interface";
            return result;
        }
        if (config_.captureMode == BackendCaptureMode::PcapOffline && !config_.pcapPath.has_value()) {
            CaptureCommandResult result;
            result.status = statusLocked();
            result.error = "pcap capture requires --pcap";
            return result;
        }

        setStateLocked(CaptureServiceState::Starting);
        ++startCount_;
        lastError_.reset();
        stopRequested_.store(false, std::memory_order_relaxed);
    }

    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    workerThread_ = std::thread(&CaptureService::runCaptureWorker, this);

    CaptureCommandResult result;
    result.accepted = true;
    result.status = status();
    return result;
}

CaptureCommandResult CaptureService::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == CaptureServiceState::Stopped && !workerThread_.joinable()) {
            CaptureCommandResult result;
            result.accepted = true;
            result.status = statusLocked();
            return result;
        }

        stopRequested_.store(true, std::memory_order_relaxed);
        if (state_ != CaptureServiceState::Error) {
            setStateLocked(CaptureServiceState::Stopping);
        }
    }

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stopCount_;
        if (state_ != CaptureServiceState::Error) {
            setStateLocked(CaptureServiceState::Stopped);
        }
    }

    CaptureCommandResult result;
    result.accepted = true;
    result.status = status();
    return result;
}

CaptureCommandResult CaptureService::restart()
{
    const auto stopResult = stop();
    if (!stopResult.accepted) {
        return stopResult;
    }
    return start();
}

CaptureServiceStatus CaptureService::status() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return statusLocked();
}

void CaptureService::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopRequested_.store(true, std::memory_order_relaxed);
        if (state_ == CaptureServiceState::Running || state_ == CaptureServiceState::Starting) {
            setStateLocked(CaptureServiceState::Stopping);
        }
    }

    if (workerThread_.joinable()
        && std::this_thread::get_id() != workerThread_.get_id()) {
        workerThread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != CaptureServiceState::Error) {
            setStateLocked(CaptureServiceState::Stopped);
        }
    }
}

void CaptureService::runCaptureWorker()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        setStateLocked(CaptureServiceState::Running);
    }

    // Publish capture.started
    EventBus::rx().publish({
        "capture.started",
        currentIso8601Timestamp(),
        "info",
        CaptureStartedEvent{statusLocked()}
    });

    try {
        storage::SQLiteWriter writer(config_.sqlitePath);
        storage::AnalysisSessionRecord session;
        session.mode = config_.captureMode == BackendCaptureMode::Live ? "Live Capture" : "PCAP Analysis";
        session.source = config_.captureMode == BackendCaptureMode::Live
            ? config_.interfaceName.value_or("")
            : config_.pcapPath.value_or("");
        session.status = "Running";
        session.storageContext = config_.sqlitePath;
        if (const auto error = writer.createAnalysisSession(session); error.has_value()) {
            throw std::runtime_error(*error);
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            activeSessionId_ = session.id;
        }

        if (config_.captureMode == BackendCaptureMode::Live) {
            runLiveCapture();
        } else {
            runPcapAnalysis();
        }

        finishSession("Completed");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            activeSessionId_ = 0;
            if (state_ != CaptureServiceState::Stopping) {
                setStateLocked(CaptureServiceState::Stopped);
            }
        }
        // Publish capture.stopped
        EventBus::rx().publish({
            "capture.stopped",
            currentIso8601Timestamp(),
            "info",
            CaptureStoppedEvent{statusLocked()}
        });
        // Publish metrics.updated because capture stops count changed
        {
            storage::SQLiteWriter writer(config_.sqlitePath);
            BackendMetricsSnapshot snapshot;
            writer.countAssets(snapshot.assetCount);
            writer.countEvents(snapshot.eventCount);
            snapshot.captureStarts = startCount_;
            snapshot.captureStops = stopCount_;
            EventBus::rx().publish({
                "metrics.updated",
                currentIso8601Timestamp(),
                "info",
                MetricsUpdatedEvent{snapshot}
            });
        }
    } catch (const std::exception& error) {
        setError(error.what());
        finishSession("Failed", error.what());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            activeSessionId_ = 0;
        }
        // Publish capture.error
        EventBus::rx().publish({
            "capture.error",
            currentIso8601Timestamp(),
            "error",
            CaptureErrorEvent{error.what()}
        });
        // Also capture.stopped is expected
        EventBus::rx().publish({
            "capture.stopped",
            currentIso8601Timestamp(),
            "info",
            CaptureStoppedEvent{statusLocked()}
        });
    } catch (...) {
        setError("capture worker failed with an unknown error");
        finishSession("Failed", "capture worker failed with an unknown error");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            activeSessionId_ = 0;
        }
        // Publish capture.error
        EventBus::rx().publish({
            "capture.error",
            currentIso8601Timestamp(),
            "error",
            CaptureErrorEvent{"capture worker failed with an unknown error"}
        });
        // Also capture.stopped is expected
        EventBus::rx().publish({
            "capture.stopped",
            currentIso8601Timestamp(),
            "info",
            CaptureStoppedEvent{statusLocked()}
        });
    }
}

void CaptureService::runLiveCapture()
{
    auto backendResult = capture::createCaptureBackend(capture::CaptureBackendSelection::Auto);
    if (backendResult.error.has_value() || !backendResult.backend) {
        throw std::runtime_error(backendResult.error.value_or("capture backend could not be created"));
    }

    capture::LiveCaptureOptions liveOptions;
    liveOptions.stopRequested = [this]() {
        return stopRequested_.load(std::memory_order_relaxed);
    };

    capture::CaptureConfig captureConfig;
    captureConfig.interfaceName = *config_.interfaceName;
    captureConfig.packetFilter = config_.packetFilter;
    captureConfig.requestedBackend = capture::CaptureBackendSelection::Auto;
    captureConfig.liveOptions = std::move(liveOptions);

    live::LivePipelineOptions pipelineOptions;
    pipelineOptions.monitorConfig = defaultMonitorConfig(*config_.interfaceName);
    pipelineOptions.eventCallback = [this](const asset::AssetEvent& event) {
        publishAssetEvent(event, config_.sqlitePath, startCount_, stopCount_);
    };
    pipelineOptions.assetCallback = [this](const asset::Asset& asset, bool isNew) {
        if (isNew) {
            persistAsset(asset, config_.sqlitePath);
        }
    };

    const auto liveResult = live::runLiveCapturePipeline(
        *backendResult.backend,
        std::move(captureConfig),
        std::move(backendResult.initialStats),
        std::move(pipelineOptions));

    if (liveResult.error.has_value()) {
        throw std::runtime_error(*liveResult.error);
    }

    storage::SQLiteWriter writer(config_.sqlitePath);
    if (const auto error = writer.writeAssets(liveResult.assets); error.has_value()) {
        throw std::runtime_error(*error);
    }
}

void CaptureService::runPcapAnalysis()
{
    capture::PacketCaptureBackend backend;
    const auto pcapResult = backend.readPcapFile(*config_.pcapPath, config_.packetFilter);
    if (pcapResult.error.has_value()) {
        throw std::runtime_error(*pcapResult.error);
    }

    monitor::AssetMonitor monitor(
        defaultMonitorConfig(constants::capture::PcapInterfaceName),
        [this](const asset::AssetEvent& event) {
            publishAssetEvent(event, config_.sqlitePath, startCount_, stopCount_);
        },
        [this](const asset::Asset& asset, bool isNew) {
            if (isNew) {
                persistAsset(asset, config_.sqlitePath);
            }
        });

    for (const auto& packet : pcapResult.packets) {
        if (stopRequested_.load(std::memory_order_relaxed)) {
            break;
        }
        if (packet.linkType != capture::LinkType::Ethernet) {
            continue;
        }
        const auto observations = parser::parseEthernetObservations(
            packet.bytes,
            toObservationTimestamp(packet.timestamp));
        for (const auto& observation : observations) {
            monitor.applyObservation(observation);
        }
    }

    storage::SQLiteWriter writer(config_.sqlitePath);
    if (const auto error = writer.writeAssets(monitor.assets()); error.has_value()) {
        throw std::runtime_error(*error);
    }
}

void CaptureService::finishSession(const std::string& status, const std::string& errorSummary)
{
    std::int64_t sessionId = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessionId = activeSessionId_;
    }
    if (sessionId <= 0) {
        return;
    }

    try {
        storage::SQLiteWriter writer(config_.sqlitePath);
        int assetCount = 0;
        int eventCount = 0;
        (void)writer.countAssets(assetCount);
        (void)writer.countEvents(eventCount);
        (void)writer.finishAnalysisSession(sessionId, status, assetCount, eventCount, errorSummary);
    } catch (const std::exception&) {
        // Preserve the original capture error; session finalization is best effort.
    }
}

CaptureServiceStatus CaptureService::statusLocked() const
{
    CaptureServiceStatus current;
    current.state = state_;
    current.stateName = captureServiceStateName(state_);
    current.mode = captureModeName(config_.captureMode);
    current.source = config_.captureMode == BackendCaptureMode::PcapOffline
        ? config_.pcapPath
        : config_.interfaceName;
    current.lastError = lastError_;
    current.startCount = startCount_;
    current.stopCount = stopCount_;
    current.running = state_ == CaptureServiceState::Running
        || state_ == CaptureServiceState::Starting;
    return current;
}

void CaptureService::setStateLocked(CaptureServiceState state)
{
    state_ = state;
}

void CaptureService::setError(std::string message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_ = std::move(message);
    setStateLocked(CaptureServiceState::Error);
}

std::string captureServiceStateName(CaptureServiceState state)
{
    switch (state) {
    case CaptureServiceState::Stopped:
        return "stopped";
    case CaptureServiceState::Starting:
        return "starting";
    case CaptureServiceState::Running:
        return "running";
    case CaptureServiceState::Stopping:
        return "stopping";
    case CaptureServiceState::Error:
        return "error";
    }
    return "stopped";
}

} // namespace asset_discovery::backend
