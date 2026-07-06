#pragma once

#include "pnad/event/AssetEvent.hpp"

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace asset_discovery::output {

class EventSink {
public:
    virtual ~EventSink() = default;
    virtual void write(const asset::AssetEvent& event) = 0;
    virtual void flush();
};

class EventDispatcher {
public:
    void addSink(std::unique_ptr<EventSink> sink);
    bool empty() const;
    void dispatch(const asset::AssetEvent& event);
    void flush();

private:
    std::vector<std::unique_ptr<EventSink>> sinks_;
};

std::string renderConsoleEvent(const asset::AssetEvent& event);

class ConsoleEventSink final : public EventSink {
public:
    explicit ConsoleEventSink(std::ostream& output);

    void write(const asset::AssetEvent& event) override;
    void flush() override;

private:
    std::ostream* output_;
};

} // namespace asset_discovery::output
