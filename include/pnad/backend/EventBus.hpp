#pragma once

#include "pnad/constants/BackendConstants.hpp"

#include "pnad/backend/QueryServices.hpp"
#include "pnad/system/BoundedQueue.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace asset_discovery::backend {

struct AssetCreatedEvent {
    BackendAssetRecord asset;
};

struct AssetUpdatedEvent {
    BackendAssetRecord asset;
};

struct EventDetectedEvent {
    BackendEventRecord event;
};

struct LogMessageEvent {
    std::string message;
};

struct CaptureStartedEvent {
    CaptureServiceStatus status;
};

struct CaptureStoppedEvent {
    CaptureServiceStatus status;
};

struct CaptureErrorEvent {
    std::string error;
};

struct MetricsUpdatedEvent {
    BackendMetricsSnapshot metrics;
};

using DomainEventData = std::variant<
    AssetCreatedEvent,
    AssetUpdatedEvent,
    EventDetectedEvent,
    LogMessageEvent,
    CaptureStartedEvent,
    CaptureStoppedEvent,
    CaptureErrorEvent,
    MetricsUpdatedEvent
>;

struct DomainEvent {
    std::string type;
    std::string timestamp;
    std::string severity;
    DomainEventData data;
};

using EventQueue = live::BoundedQueue<DomainEvent>;
using EventQueuePtr = std::shared_ptr<EventQueue>;

inline std::string currentIso8601Timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc {};
    gmtime_r(&time, &tm_utc);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    return std::string(buffer);
}

class EventBus {
public:
    static EventBus& rx()
    {
        static EventBus instance;
        return instance;
    }

    EventBus() = default;

    EventQueuePtr subscribe(std::size_t capacity = constants::backend::DefaultEventBusCapacity)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto queue = std::make_shared<EventQueue>(capacity);
        subscribers_.push_back(queue);
        return queue;
    }

    void unsubscribe(const EventQueuePtr& queue)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.erase(
            std::remove(subscribers_.begin(), subscribers_.end(), queue),
            subscribers_.end());
    }

    void publish(DomainEvent event)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) {
            return;
        }
        for (auto& queue : subscribers_) {
            auto result = queue->tryPush(event);
            if (result == live::QueuePushResult::Full) {
                droppedCount_++;
            }
        }
    }

    void shutdown()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        for (auto& queue : subscribers_) {
            queue->close();
        }
        subscribers_.clear();
    }

    std::uint64_t droppedCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return droppedCount_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<EventQueuePtr> subscribers_;
    bool closed_ = false;
    std::uint64_t droppedCount_ = 0;
};

inline void logBackendMessage(const std::string& runtimeLogPath, const std::string& message)
{
    if (!runtimeLogPath.empty()) {
        std::ofstream file(runtimeLogPath, std::ios::app);
        if (file) {
            file << message << "\n";
        }
    }
    EventBus::rx().publish({
        "log.message",
        currentIso8601Timestamp(),
        "info",
        LogMessageEvent{message}
    });
}

} // namespace asset_discovery::backend
