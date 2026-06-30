#include "application/live/BoundedQueue.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using asset_discovery::live::BoundedQueue;
using asset_discovery::live::QueuePushResult;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void pushesAndPopsInOrder()
{
    BoundedQueue<int> queue(2);

    expect(queue.tryPush(10) == QueuePushResult::Pushed, "first push should succeed");
    expect(queue.tryPush(20) == QueuePushResult::Pushed, "second push should succeed");
    expect(queue.highWatermark() == 2, "high watermark should track maximum size");

    int value = 0;
    expect(queue.waitPop(value), "first pop should succeed");
    expect(value == 10, "first pop should return first value");
    expect(queue.waitPop(value), "second pop should succeed");
    expect(value == 20, "second pop should return second value");
}

void reportsFullQueue()
{
    BoundedQueue<int> queue(1);

    expect(queue.tryPush(1) == QueuePushResult::Pushed, "initial push should succeed");
    expect(queue.tryPush(2) == QueuePushResult::Full, "push should report full queue");
    expect(queue.size() == 1, "full push should not grow queue");
    expect(queue.highWatermark() == 1, "full push should not change high watermark");
}

void drainsAfterClose()
{
    BoundedQueue<int> queue(2);
    queue.tryPush(7);
    queue.close();

    int value = 0;
    expect(queue.closed(), "queue should report closed after close");
    expect(queue.waitPop(value), "closed queue should drain existing values");
    expect(value == 7, "drained value should be preserved");
    expect(!queue.waitPop(value), "closed empty queue should stop waitPop");
    expect(queue.tryPush(8) == QueuePushResult::Closed, "tryPush should reject after close");
    expect(!queue.waitPush(9), "waitPush should reject after close");
}

void waitPushUnblocksWhenConsumerPops()
{
    BoundedQueue<int> queue(1);
    queue.tryPush(1);

    bool pushed = false;
    std::thread producer([&] {
        pushed = queue.waitPush(2);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    int value = 0;
    expect(queue.waitPop(value), "consumer should pop existing value");
    expect(value == 1, "consumer should receive initial value");

    producer.join();
    expect(pushed, "waitPush should complete after space becomes available");

    expect(queue.waitPop(value), "consumer should pop producer value");
    expect(value == 2, "consumer should receive producer value");
}

void waitPopUnblocksOnClose()
{
    BoundedQueue<int> queue(1);
    bool popped = true;
    std::thread consumer([&] {
        int value = 0;
        popped = queue.waitPop(value);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    queue.close();
    consumer.join();

    expect(!popped, "waitPop should return false when closed while empty");
}

} // namespace

int main()
{
    pushesAndPopsInOrder();
    reportsFullQueue();
    drainsAfterClose();
    waitPushUnblocksWhenConsumerPops();
    waitPopUnblocksOnClose();

    if (failures != 0) {
        std::cerr << failures << " bounded queue test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
