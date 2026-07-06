#include "pnad/cli/Arguments.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::cli::parseArguments;
using asset_discovery::cli::usageText;
using asset_discovery::cli::versionText;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void expectErrorContains(
    const std::vector<std::string>& args,
    const std::string& expected,
    const std::string& message)
{
    const auto result = parseArguments(args);
    expect(result.error.has_value(), message);
    if (result.error.has_value()) {
        expect(result.error->find(expected) != std::string::npos,
            "error should contain: " + expected);
    }
}

void parsesPcapMode()
{
    const auto result = parseArguments({"--pcap", "samples/arp.pcap"});
    expect(!result.error.has_value(), "valid --pcap mode should be accepted");
    expect(result.options.captureMode == asset_discovery::cli::CaptureMode::PcapOffline,
        "pcap capture mode should be selected");
}

void parsesRetainedControls()
{
    const auto result = parseArguments({
        "--pcap",
        "samples/arp.pcap",
        "--filter",
        "arp or udp port 67 or udp port 68",
        "--output",
        "json",
    });

    expect(!result.error.has_value(), "retained controls should be accepted");
    expect(result.options.packetFilter == "arp or udp port 67 or udp port 68",
        "packet filter should be stored");
    expect(result.options.outputFormat == asset_discovery::cli::OutputFormat::Json,
        "json output format should be stored");
}

void defersSourceSelectionValidation()
{
    const auto missing = parseArguments({});
    expect(!missing.error.has_value(), "missing source should be validated after config merge");
    expect(!missing.options.captureMode.has_value(), "missing source should not select capture mode");

    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--interface", "eth0"},
        "--interface has been removed",
        "removed --interface should be rejected");
}

void parsesVersion()
{
    const auto version = parseArguments({"--version"});
    expect(!version.error.has_value(), "--version should parse without input");
    expect(version.options.versionRequested, "version request should be tracked");
    expect(versionText().find("asset-discovery ") == 0, "version text should include executable name");
}

void rejectsRemovedConfigControls()
{
    expectErrorContains(
        {"--config", "configs/default.yaml", "--pcap", "samples/arp.pcap"},
        "--config has been removed",
        "removed --config should be rejected");
    expectErrorContains(
        {"--profile", "pcap", "--pcap", "samples/arp.pcap"},
        "--profile has been removed",
        "removed --profile should be rejected");
}

void rejectsInvalidRetainedControls()
{
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--filter", ""},
        "--filter cannot be empty",
        "empty --filter should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--capture-backend", "pcap"},
        "--capture-backend has been removed",
        "removed backend should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--output", "xml"},
        "expected one of: table, json, csv",
        "unknown output format should be rejected");
}

void rejectsRemovedLiveFlags()
{
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--duration", "60"},
        "--duration has been removed",
        "removed --duration should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--live"},
        "--live has been removed",
        "removed --live should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--idle-timeout", "30"},
        "--idle-timeout has been removed",
        "removed --idle-timeout should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--max-assets", "10"},
        "--max-assets has been removed",
        "removed --max-assets should be rejected");
}

void rejectsRemovedDatabaseAndEventFlags()
{
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--db-url", "old-db-url"},
        "--db-url has been removed",
        "removed --db-url should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--events", "stdout"},
        "--events has been removed",
        "removed --events should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--events-json", "old-event-path"},
        "--events-json has been removed",
        "removed --events-json should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--syslog"},
        "--syslog has been removed",
        "removed --syslog should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--events-db"},
        "--events-db has been removed",
        "removed --events-db should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--event-rate-limit", "60"},
        "--event-rate-limit has been removed",
        "removed --event-rate-limit should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--event-queue-capacity", "2048"},
        "--event-queue-capacity has been removed",
        "removed --event-queue-capacity should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--flip-flop-window", "30"},
        "--flip-flop-window has been removed",
        "removed --flip-flop-window should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--reappearance-threshold", "300"},
        "--reappearance-threshold has been removed",
        "removed --reappearance-threshold should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--local-net", "192.168.1.0/24"},
        "--local-net has been removed",
        "removed --local-net should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--ignore-net", "127.0.0.0/8"},
        "--ignore-net has been removed",
        "removed --ignore-net should be rejected");
}

void usageShowsSimplifiedForms()
{
    const auto usage = usageText("asset-discovery");
    expect(usage.find("--pcap <file> [--filter <bpf>]") != std::string::npos,
        "usage should show pcap-only form");
    expect(usage.find("--interface") == std::string::npos,
        "usage should not show live capture");
    expect(usage.find("--config") == std::string::npos,
        "usage should not show explicit config");
    expect(usage.find("--profile") == std::string::npos,
        "usage should not show profile");
    expect(usage.find("Common options:") != std::string::npos,
        "usage should show common options section");
    expect(usage.find("Advanced overrides:") == std::string::npos,
        "usage should not show removed advanced overrides section");
    expect(usage.find("--version") != std::string::npos,
        "usage should show version option");
    expect(usage.find("--duration") == std::string::npos,
        "usage should not mention --duration");
    expect(usage.find("--events-json") == std::string::npos,
        "usage should not mention removed event sink flags");
    expect(usage.find("ASSET_DISCOVERY_EVENTS_JSON") == std::string::npos,
        "usage should not document removed event path environment override");
}

} // namespace

int main()
{
    parsesPcapMode();
    parsesRetainedControls();
    defersSourceSelectionValidation();
    parsesVersion();
    rejectsRemovedConfigControls();
    rejectsInvalidRetainedControls();
    rejectsRemovedLiveFlags();
    rejectsRemovedDatabaseAndEventFlags();
    usageShowsSimplifiedForms();

    if (failures > 0) {
        std::cerr << failures << " argument test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
