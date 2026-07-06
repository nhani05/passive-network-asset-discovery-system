#include "pnad/backend/CaptureService.hpp"
#include "pnad/backend/EventBus.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

asset_discovery::backend::CaptureServiceStatus waitForTerminalStatus(
    asset_discovery::backend::CaptureService& service)
{
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto status = service.status();
        if (status.state == asset_discovery::backend::CaptureServiceState::Stopped
            || status.state == asset_discovery::backend::CaptureServiceState::Error) {
            return status;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return service.status();
}

void rejectsLiveStartWithoutInterface()
{
    asset_discovery::backend::BackendConfig config;
    config.captureMode = asset_discovery::backend::BackendCaptureMode::Live;
    asset_discovery::backend::CaptureService service(config);

    const auto result = service.start();
    expect(!result.accepted, "live start without interface should be rejected");
    expect(result.error.has_value(), "rejected live start should report an error");
    expect(service.status().state == asset_discovery::backend::CaptureServiceState::Stopped,
        "rejected live start should leave service stopped");
}

void stopAndShutdownAreIdempotent()
{
    asset_discovery::backend::BackendConfig config;
    asset_discovery::backend::CaptureService service(config);

    const auto firstStop = service.stop();
    const auto secondStop = service.stop();
    service.shutdown();
    service.shutdown();

    expect(firstStop.accepted, "first stop should be accepted");
    expect(secondStop.accepted, "second stop should be accepted");
    expect(service.status().state == asset_discovery::backend::CaptureServiceState::Stopped,
        "shutdown should leave service stopped");
}

void recordsWorkerFailure()
{
    asset_discovery::backend::BackendConfig config;
    config.captureMode = asset_discovery::backend::BackendCaptureMode::PcapOffline;
    config.pcapPath = std::string(PNAD_SOURCE_DIR) + "/samples/missing.pcap";
    config.sqlitePath = "backend-service-failure-test.db";
    asset_discovery::backend::CaptureService service(config);

    const auto start = service.start();
    expect(start.accepted, "pcap worker start should be accepted before worker failure");

    const auto status = waitForTerminalStatus(service);
    expect(status.state == asset_discovery::backend::CaptureServiceState::Error,
        "missing pcap should move service into error state");
    expect(status.lastError.has_value(), "worker failure should be recorded");
    service.shutdown();
}

void testEventBusMultiSubscriber()
{
    using namespace asset_discovery::backend;
    EventBus bus;

    auto q1 = bus.subscribe(10);
    auto q2 = bus.subscribe(10);

    DomainEvent ev;
    ev.type = "test.event";
    ev.timestamp = "2026-07-04T00:00:00Z";
    ev.severity = "info";
    ev.data = LogMessageEvent{"hello"};

    bus.publish(ev);

    DomainEvent out1, out2;
    expect(q1->waitPop(out1), "q1 should receive event");
    expect(q2->waitPop(out2), "q2 should receive event");
    expect(out1.type == "test.event", "event type should match");
    expect(out2.type == "test.event", "event type should match");

    bus.unsubscribe(q1);
    bus.unsubscribe(q2);
}

void testEventBusQueueCapacity()
{
    using namespace asset_discovery::backend;
    EventBus bus;

    auto q1 = bus.subscribe(1);

    DomainEvent ev;
    ev.type = "test.event";
    ev.data = LogMessageEvent{"hello"};

    bus.publish(ev);
    bus.publish(ev);

    expect(bus.droppedCount() == 1, "should record 1 dropped event");
    expect(q1->size() == 1, "q1 size should be capped at 1");

    bus.shutdown();
}

void testEventBusShutdown()
{
    using namespace asset_discovery::backend;
    EventBus bus;

    auto q1 = bus.subscribe(10);

    bus.shutdown();

    DomainEvent out;
    expect(!q1->waitPop(out), "waitPop should return false after shutdown");
    expect(q1->closed(), "queue should be closed after shutdown");
}

} // namespace

int main()
{
    rejectsLiveStartWithoutInterface();
    stopAndShutdownAreIdempotent();
    recordsWorkerFailure();
    testEventBusMultiSubscriber();
    testEventBusQueueCapacity();
    testEventBusShutdown();
    std::cout << "Backend service tests passed\n";
    return 0;
}
