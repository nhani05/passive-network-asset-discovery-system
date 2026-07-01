#include "infrastructure/output/EventSink.hpp"

#include <ostream>
#include <sstream>
#include <utility>

#if defined(__linux__) || defined(__APPLE__)
#include <syslog.h>
#define ASSET_DISCOVERY_HAS_SYSLOG 1
#else
#define ASSET_DISCOVERY_HAS_SYSLOG 0
#endif

namespace asset_discovery::output {
namespace {

void appendKeyValue(std::ostringstream& output, const std::string& key, const std::optional<std::string>& value)
{
    if (value.has_value() && !value->empty()) {
        output << ' ' << key << '=' << *value;
    }
}

std::string jsonEscape(const std::string& value)
{
    std::string output;
    for (const char character : value) {
        switch (character) {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output += character;
            break;
        }
    }
    return output;
}

void appendJsonStringField(
    std::ostringstream& output,
    bool& first,
    const std::string& key,
    const std::string& value)
{
    if (!first) {
        output << ',';
    }
    output << '"' << jsonEscape(key) << "\":\"" << jsonEscape(value) << '"';
    first = false;
}

void appendJsonOptionalField(
    std::ostringstream& output,
    bool& first,
    const std::string& key,
    const std::optional<std::string>& value)
{
    if (value.has_value()) {
        appendJsonStringField(output, first, key, *value);
    }
}

std::string metadataJson(const std::map<std::string, std::string>& metadata)
{
    std::ostringstream output;
    output << '{';
    bool first = true;
    for (const auto& item : metadata) {
        appendJsonStringField(output, first, item.first, item.second);
    }
    output << '}';
    return output.str();
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
    appendKeyValue(output, "old_ip", event.oldIpAddress);
    appendKeyValue(output, "new_ip", event.newIpAddress);
    appendKeyValue(output, "old_mac", event.oldMacAddress);
    appendKeyValue(output, "new_mac", event.newMacAddress);
    appendKeyValue(output, "hostname", event.hostname);
    if (!event.protocol.empty()) {
        output << " protocol=" << event.protocol;
    }
    if (!event.interfaceName.empty()) {
        output << " iface=" << event.interfaceName;
    }
    return output.str();
}

std::string renderEventNdjson(const asset::AssetEvent& event)
{
    std::ostringstream output;
    output << '{';
    bool first = true;
    appendJsonStringField(output, first, "ts", asset::formatEventTimestamp(event.timestamp));
    appendJsonStringField(output, first, "event_type", asset::assetEventTypeName(event.type));
    appendJsonStringField(output, first, "severity", asset::assetEventSeverityName(event.severity));
    appendJsonOptionalField(output, first, "ip", event.ipAddress);
    appendJsonOptionalField(output, first, "mac", event.macAddress);
    appendJsonOptionalField(output, first, "old_ip", event.oldIpAddress);
    appendJsonOptionalField(output, first, "new_ip", event.newIpAddress);
    appendJsonOptionalField(output, first, "old_mac", event.oldMacAddress);
    appendJsonOptionalField(output, first, "new_mac", event.newMacAddress);
    appendJsonOptionalField(output, first, "hostname", event.hostname);
    if (!event.protocol.empty()) {
        appendJsonStringField(output, first, "protocol", event.protocol);
    }
    if (!event.interfaceName.empty()) {
        appendJsonStringField(output, first, "interface", event.interfaceName);
    }
    if (!event.message.empty()) {
        appendJsonStringField(output, first, "message", event.message);
    }
    if (!first) {
        output << ',';
    }
    output << "\"metadata\":" << metadataJson(event.metadata);
    output << '}';
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

NdjsonEventSink::NdjsonEventSink(const std::string& path)
    : path_(path)
    , file_(path, std::ios::app)
{
}

bool NdjsonEventSink::ok() const
{
    return file_.is_open() && file_.good();
}

std::string NdjsonEventSink::error() const
{
    return "could not open event NDJSON file '" + path_ + "'";
}

void NdjsonEventSink::write(const asset::AssetEvent& event)
{
    file_ << renderEventNdjson(event) << '\n';
}

void NdjsonEventSink::flush()
{
    file_.flush();
}

int syslogPriorityForSeverity(asset::AssetEventSeverity severity)
{
#if ASSET_DISCOVERY_HAS_SYSLOG == 1
    switch (severity) {
    case asset::AssetEventSeverity::Debug:
        return LOG_DEBUG;
    case asset::AssetEventSeverity::Info:
        return LOG_INFO;
    case asset::AssetEventSeverity::Warning:
        return LOG_WARNING;
    case asset::AssetEventSeverity::High:
        return LOG_ALERT;
    }
    return LOG_INFO;
#else
    (void)severity;
    return 0;
#endif
}

SyslogEventSink::SyslogEventSink(std::string identifier)
    : identifier_(std::move(identifier))
{
#if ASSET_DISCOVERY_HAS_SYSLOG == 1
    openlog(identifier_.c_str(), LOG_PID | LOG_CONS, LOG_DAEMON);
    opened_ = true;
#endif
}

SyslogEventSink::~SyslogEventSink()
{
#if ASSET_DISCOVERY_HAS_SYSLOG == 1
    if (opened_) {
        closelog();
    }
#endif
}

bool SyslogEventSink::supported()
{
    return ASSET_DISCOVERY_HAS_SYSLOG == 1;
}

bool SyslogEventSink::ok() const
{
    return supported() && opened_;
}

std::string SyslogEventSink::error() const
{
    return "syslog event output is not supported on this platform";
}

void SyslogEventSink::write(const asset::AssetEvent& event)
{
#if ASSET_DISCOVERY_HAS_SYSLOG == 1
    syslog(syslogPriorityForSeverity(event.severity), "%s", renderConsoleEvent(event).c_str());
#else
    (void)event;
#endif
}

} // namespace asset_discovery::output
