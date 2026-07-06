#include "pnad/event/EventSink.hpp"

#include <ostream>
#include <sstream>
#include <utility>

namespace asset_discovery::output {
namespace {

void appendKeyValue(std::ostringstream& output, const std::string& key, const std::optional<std::string>& value)
{
    if (value.has_value() && !value->empty()) {
        output << ' ' << key << '=' << *value;
    }
}

} // namespace

void EventSink::flush()
{
}

void EventDispatcher::addSink(std::unique_ptr<EventSink> sink)
{
    if (sink) {
        sinks_.push_back(std::move(sink));
    }
}

bool EventDispatcher::empty() const
{
    return sinks_.empty();
}

void EventDispatcher::dispatch(const asset::AssetEvent& event)
{
    for (auto& sink : sinks_) {
        sink->write(event);
    }
}

void EventDispatcher::flush()
{
    for (auto& sink : sinks_) {
        sink->flush();
    }
}

std::string renderConsoleEvent(const asset::AssetEvent& event)
{
    std::ostringstream output;
    output << asset::formatEventTimestamp(event.timestamp)
           << ' ' << asset::assetEventSeverityLabel(event.severity)
           << ' ' << asset::assetEventTypeName(event.type);
    appendKeyValue(output, "ip", event.ipAddress);
    appendKeyValue(output, "mac", event.macAddress);
    appendKeyValue(output, "hostname", event.hostname);
    if (!event.protocol.empty()) {
        output << " protocol=" << event.protocol;
    }
    if (!event.interfaceName.empty()) {
        output << " iface=" << event.interfaceName;
    }
    return output.str();
}

ConsoleEventSink::ConsoleEventSink(std::ostream& output)
    : output_(&output)
{
}

void ConsoleEventSink::write(const asset::AssetEvent& event)
{
    *output_ << renderConsoleEvent(event) << '\n';
}

void ConsoleEventSink::flush()
{
    output_->flush();
}

} // namespace asset_discovery::output
