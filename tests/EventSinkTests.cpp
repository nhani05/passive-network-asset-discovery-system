#include "pnad/event/EventSink.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

using asset_discovery::asset::AssetEvent;
using asset_discovery::asset::AssetEventSeverity;
using asset_discovery::asset::AssetEventType;
using asset_discovery::output::ConsoleEventSink;
using asset_discovery::output::EventDispatcher;
using asset_discovery::output::EventSink;
using asset_discovery::output::NdjsonEventSink;
using asset_discovery::output::renderConsoleEvent;
using asset_discovery::output::renderEventNdjson;
using asset_discovery::output::syslogPriorityForSeverity;
using asset_discovery::output::SyslogEventSink;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

bool contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

AssetEvent sampleEvent()
{
    AssetEvent event;
    event.timestamp = {100, 200};
    event.type = AssetEventType::MacChangedForIp;
    event.severity = AssetEventSeverity::Warning;
    event.ipAddress = "192.168.1.12";
    event.macAddress = "11:22:33:44:55:66";
    event.oldMacAddress = "aa:bb:cc:dd:ee:ff";
    event.newMacAddress = "11:22:33:44:55:66";
    event.protocol = "arp";
    event.interfaceName = "eth0";
    event.message = "IP address is now associated with a different MAC address";
    event.metadata["note"] = "quoted \"value\"";
    return event;
}

class CollectingSink final : public EventSink {
public:
    void write(const AssetEvent& event) override
    {
        events.push_back(event);
    }

    std::vector<AssetEvent> events;
};

void rendersConsoleEvent()
{
    const auto line = renderConsoleEvent(sampleEvent());
    expect(contains(line, "100.200 WARN mac_changed_for_ip"), "console line should contain timestamp/severity/type");
    expect(contains(line, "ip=192.168.1.12"), "console line should contain IP");
    expect(contains(line, "old_mac=aa:bb:cc:dd:ee:ff"), "console line should contain old MAC");
    expect(contains(line, "new_mac=11:22:33:44:55:66"), "console line should contain new MAC");
    expect(contains(line, "protocol=arp"), "console line should contain protocol");
    expect(contains(line, "iface=eth0"), "console line should contain interface");
}

void rendersNdjsonEvent()
{
    const auto json = renderEventNdjson(sampleEvent());
    expect(contains(json, "\"event_type\":\"mac_changed_for_ip\""), "NDJSON should contain event type");
    expect(contains(json, "\"severity\":\"warning\""), "NDJSON should contain severity");
    expect(contains(json, "\"old_mac\":\"aa:bb:cc:dd:ee:ff\""), "NDJSON should contain old MAC");
    expect(contains(json, "\"new_mac\":\"11:22:33:44:55:66\""), "NDJSON should contain new MAC");
    expect(contains(json, "\"metadata\":{\"note\":\"quoted \\\"value\\\"\"}"), "NDJSON should escape metadata");
}

void dispatchesToMultipleSinks()
{
    auto first = std::make_unique<CollectingSink>();
    auto second = std::make_unique<CollectingSink>();
    auto* firstPtr = first.get();
    auto* secondPtr = second.get();

    EventDispatcher dispatcher;
    dispatcher.addSink(std::move(first));
    dispatcher.addSink(std::move(second));
    dispatcher.dispatch(sampleEvent());

    expect(firstPtr->events.size() == 1, "first sink should receive event");
    expect(secondPtr->events.size() == 1, "second sink should receive event");
}

void writesConsoleSink()
{
    std::ostringstream output;
    ConsoleEventSink sink(output);
    sink.write(sampleEvent());
    sink.flush();
    expect(contains(output.str(), "mac_changed_for_ip"), "console sink should write rendered event");
}

void writesNdjsonFileSink()
{
    const std::string path = "event-sink-test.ndjson";
    std::remove(path.c_str());
    {
        NdjsonEventSink sink(path);
        expect(sink.ok(), "NDJSON sink should open writable file");
        sink.write(sampleEvent());
        sink.flush();
    }

    std::ifstream input(path);
    std::string line;
    std::getline(input, line);
    expect(contains(line, "\"event_type\":\"mac_changed_for_ip\""), "NDJSON sink should write event line");
    std::remove(path.c_str());
}

void mapsSyslogSeverity()
{
    if (SyslogEventSink::supported()) {
        expect(syslogPriorityForSeverity(AssetEventSeverity::Warning)
            != syslogPriorityForSeverity(AssetEventSeverity::Debug),
            "syslog severity mapping should distinguish warning and debug where supported");
    } else {
        expect(syslogPriorityForSeverity(AssetEventSeverity::Warning) == 0,
            "unsupported syslog builds should return neutral priority");
    }
}

} // namespace

int main()
{
    rendersConsoleEvent();
    rendersNdjsonEvent();
    dispatchesToMultipleSinks();
    writesConsoleSink();
    writesNdjsonFileSink();
    mapsSyslogSeverity();

    if (failures > 0) {
        std::cerr << failures << " event sink test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
