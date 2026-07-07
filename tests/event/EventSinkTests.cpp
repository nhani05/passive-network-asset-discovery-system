#include "pnad/event/EventSink.hpp"

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
using asset_discovery::output::renderConsoleEvent;

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
    event.type = AssetEventType::NewAsset;
    event.severity = AssetEventSeverity::Info;
    event.ipAddress = "192.168.1.12";
    event.macAddress = "11:22:33:44:55:66";
    event.protocol = "arp";
    event.interfaceName = "eth0";
    event.message = "New asset discovered";
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
    expect(contains(line, "100.200 INFO new_asset"), "console line should contain timestamp/severity/type");
    expect(contains(line, "ip=192.168.1.12"), "console line should contain IP");
    expect(contains(line, "protocol=arp"), "console line should contain protocol");
    expect(contains(line, "iface=eth0"), "console line should contain interface");
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
    expect(contains(output.str(), "new_asset"), "console sink should write rendered event");
}

} // namespace

int main()
{
    rendersConsoleEvent();
    dispatchesToMultipleSinks();
    writesConsoleSink();

    if (failures > 0) {
        std::cerr << failures << " event sink test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
