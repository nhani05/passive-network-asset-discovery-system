#pragma once

#include "pnad/backend/BackendConfig.hpp"

#include <cstdint>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace asset_discovery::backend {

enum class CaptureServiceState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Error,
};

struct CaptureServiceStatus {
    CaptureServiceState state = CaptureServiceState::Stopped;
    std::string stateName = "stopped";
    std::string mode = "live";
    std::optional<std::string> source;
    std::optional<std::string> lastError;
    std::uint64_t startCount = 0;
    std::uint64_t stopCount = 0;
    bool running = false;
};

struct CaptureCommandResult {
    bool accepted = false;
    CaptureServiceStatus status;
    std::optional<std::string> error;
};

class CaptureService {
public:
    explicit CaptureService(BackendConfig config);
    ~CaptureService();

    CaptureService(const CaptureService&) = delete;
    CaptureService& operator=(const CaptureService&) = delete;

    CaptureCommandResult start();
    CaptureCommandResult stop();
    CaptureCommandResult restart();
    CaptureServiceStatus status() const;
    void shutdown();

private:
    void runCaptureWorker();
    void runLiveCapture();
    void runPcapAnalysis();
    void finishSession(const std::string& status, const std::string& errorSummary = "");
    CaptureServiceStatus statusLocked() const;
    void setStateLocked(CaptureServiceState state);
    void setError(std::string message);

    BackendConfig config_;
    mutable std::mutex mutex_;
    CaptureServiceState state_ = CaptureServiceState::Stopped;
    std::optional<std::string> lastError_;
    std::uint64_t startCount_ = 0;
    std::uint64_t stopCount_ = 0;
    std::int64_t activeSessionId_ = 0;
    std::atomic_bool stopRequested_{false};
    std::thread workerThread_;
};

std::string captureServiceStateName(CaptureServiceState state);

} // namespace asset_discovery::backend
