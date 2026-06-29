#include "application/parser/ParserEngine.hpp"
#include "application/parser/ParserInterface.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using asset_discovery::parser::AssetObservation;
using asset_discovery::parser::PacketContext;
using asset_discovery::parser::ParserEngine;
using asset_discovery::parser::ParserInterface;
using asset_discovery::parser::ParserMatch;
using asset_discovery::parser::ParserRegistry;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

class StubPlugin final : public ParserInterface {
public:
    StubPlugin(std::string id, std::uint8_t score, std::vector<AssetObservation> observations, std::shared_ptr<int> parseCalls)
        : id_(std::move(id))
        , score_(score)
        , observations_(std::move(observations))
        , parseCalls_(std::move(parseCalls))
    {
    }

    std::string id() const override
    {
        return id_;
    }

    ParserMatch match(const PacketContext&) const override
    {
        return {score_};
    }

    std::vector<AssetObservation> parse(const PacketContext&) const override
    {
        ++(*parseCalls_);
        return observations_;
    }

private:
    std::string id_;
    std::uint8_t score_;
    std::vector<AssetObservation> observations_;
    std::shared_ptr<int> parseCalls_;
};

AssetObservation observationWithSource(std::string sourceId)
{
    AssetObservation observation;
    observation.macAddress = "02:42:ac:11:00:05";
    observation.sourceId = std::move(sourceId);
    return observation;
}

void skipsNonMatchingPlugins()
{
    auto calls = std::make_shared<int>(0);
    ParserRegistry registry;
    registry.registerParser(std::make_unique<StubPlugin>(
        "skip", 0, std::vector<AssetObservation>{observationWithSource("skip")}, calls));

    ParserEngine engine(std::move(registry));
    const auto observations = engine.parse(PacketContext{});

    expect(observations.empty(), "non-matching plugin should not emit observations");
    expect(*calls == 0, "non-matching plugin parse should not be called");
}

void aggregatesMatchingPluginObservations()
{
    auto firstCalls = std::make_shared<int>(0);
    auto secondCalls = std::make_shared<int>(0);
    ParserRegistry registry;
    registry.registerParser(std::make_unique<StubPlugin>(
        "first", 10, std::vector<AssetObservation>{observationWithSource("first")}, firstCalls));
    registry.registerParser(std::make_unique<StubPlugin>(
        "second", 20, std::vector<AssetObservation>{observationWithSource("second")}, secondCalls));

    ParserEngine engine(std::move(registry));
    const auto observations = engine.parse(PacketContext{});

    expect(observations.size() == 2, "matching plugin observations should be aggregated");
    expect(*firstCalls == 1, "first matching plugin parse should be called once");
    expect(*secondCalls == 1, "second matching plugin parse should be called once");
}

void handlesEmptyParseResults()
{
    auto calls = std::make_shared<int>(0);
    ParserRegistry registry;
    registry.registerParser(std::make_unique<StubPlugin>(
        "empty", 10, std::vector<AssetObservation>{}, calls));

    ParserEngine engine(std::move(registry));
    const auto observations = engine.parse(PacketContext{});

    expect(observations.empty(), "matching plugin with empty parse result should be safe");
    expect(*calls == 1, "matching plugin parse should be called once");
}

} // namespace

int main()
{
    skipsNonMatchingPlugins();
    aggregatesMatchingPluginObservations();
    handlesEmptyParseResults();

    if (failures > 0) {
        std::cerr << failures << " parser engine test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
