#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

namespace asset_discovery::live {

enum class QueuePushResult {
    Pushed,
    Full,
    Closed,
};

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    QueuePushResult tryPush(T value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) {
            return QueuePushResult::Closed;
        }
        if (queue_.size() >= capacity_) {
            return QueuePushResult::Full;
        }

        queue_.push_back(std::move(value));
        updateHighWatermark();
        notEmpty_.notify_one();
        return QueuePushResult::Pushed;
    }

    bool waitPush(T value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        notFull_.wait(lock, [this] {
            return closed_ || queue_.size() < capacity_;
        });
        if (closed_) {
            return false;
        }

        queue_.push_back(std::move(value));
        updateHighWatermark();
        notEmpty_.notify_one();
        return true;
    }

    bool waitPop(T& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmpty_.wait(lock, [this] {
            return closed_ || !queue_.empty();
        });
        if (queue_.empty()) {
            return false;
        }

        value = std::move(queue_.front());
        queue_.pop_front();
        notFull_.notify_one();
        return true;
    }

    void close()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    bool closed() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    std::size_t highWatermark() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return highWatermark_;
    }

    std::size_t capacity() const
    {
        return capacity_;
    }

private:
    void updateHighWatermark()
    {
        if (queue_.size() > highWatermark_) {
            highWatermark_ = queue_.size();
        }
    }

    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    std::deque<T> queue_;
    std::size_t highWatermark_ = 0;
    bool closed_ = false;
};

} // namespace asset_discovery::live
