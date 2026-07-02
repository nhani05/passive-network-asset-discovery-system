#include "pnad/cli/Arguments.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::capture::CaptureBackendSelection;
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

void parsesImplicitLiveMode()
{
    const auto result = parseArguments({"--interface", "eth0"});
    expect(!result.error.has_value(), "--interface should select live capture");
    expect(result.options.captureMode == asset_discovery::cli::CaptureMode::Live,
        "live capture mode should be selected");
    expect(result.options.interfaceName == "eth0", "interface name should be stored");
}

void parsesRetainedLiveControls()
{
    const auto result = parseArguments({
        "--interface",
        "eth0",
        "--filter",
        "arp or udp port 67 or udp port 68",
        "--capture-backend",
        "af-packet",
        "--output",
        "json",
        "--event-rate-limit",
        "60",
        "--event-queue-capacity",
        "2048",
        "--flip-flop-window",
        "30",
        "--reappearance-threshold",
        "300",
        "--local-net",
        "192.168.1.0/24",
        "--ignore-net",
        "192.168.1.100/32",
    });

    expect(!result.error.has_value(), "retained live tuning options should be accepted");
    expect(result.options.packetFilter == "arp or udp port 67 or udp port 68",
        "packet filter should be stored");
    expect(result.options.captureBackend == CaptureBackendSelection::AfPacket,
        "af-packet backend selection should be stored");
    expect(result.options.outputFormat == asset_discovery::cli::OutputFormat::Json,
        "json output format should be stored");
    expect(result.options.eventRateLimitSeconds == 60, "event rate limit should be stored");
    expect(result.options.eventQueueCapacity == 2048, "event queue capacity should be stored");
    expect(result.options.flipFlopWindowSeconds == 30, "flip-flop window should be stored");
    expect(result.options.reappearanceThresholdSeconds == 300, "reappearance threshold should be stored");
    expect(result.options.localNetworks.size() == 1, "local network should be stored");
    expect(result.options.ignoredNetworks.size() == 1, "ignored network should be stored");
}

void defersSourceSelectionValidation()
{
    const auto missing = parseArguments({});
    expect(!missing.error.has_value(), "missing source should be validated after config merge");
    expect(!missing.options.captureMode.has_value(), "missing source should not select capture mode");

    const auto conflicting = parseArguments({"--pcap", "samples/arp.pcap", "--interface", "eth0"});
    expect(!conflicting.error.has_value(), "conflicting sources should be validated after config merge");
    expect(!conflicting.options.captureMode.has_value(), "conflicting sources should not select capture mode");

    const auto backendWithPcap = parseArguments({"--pcap", "samples/arp.pcap", "--capture-backend", "pcap"});
    expect(!backendWithPcap.error.has_value(), "backend/source relationship should be validated after config merge");
    expect(backendWithPcap.options.captureBackendProvided, "backend presence should be tracked");
}

void parsesConfigProfileAndVersion()
{
    const auto config = parseArguments({"--config", "configs/live.yaml", "--interface", "eth0"});
    expect(!config.error.has_value(), "--config should parse");
    expect(config.options.configPath == "configs/live.yaml", "config path should be stored");

    const auto profile = parseArguments({"--profile", "live", "--interface", "eth0"});
    expect(!profile.error.has_value(), "--profile should parse");
    expect(profile.options.profileName == "live", "profile name should be stored");

    const auto version = parseArguments({"--version"});
    expect(!version.error.has_value(), "--version should parse without input");
    expect(version.options.versionRequested, "version request should be tracked");
    expect(versionText().find("asset-discovery ") == 0, "version text should include executable name");
}

void rejectsConfigProfileConflict()
{
    expectErrorContains(
        {"--config", "configs/live.yaml", "--profile", "live", "--interface", "eth0"},
        "--config and --profile cannot be combined",
        "config/profile conflict should be rejected");
}

void rejectsInvalidRetainedControls()
{
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--filter", ""},
        "--filter cannot be empty",
        "empty --filter should be rejected");
    expectErrorContains(
        {"--interface", "eth0", "--capture-backend", "raw"},
        "expected one of: auto, pcap, af-packet",
        "unknown backend should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--output", "xml"},
        "expected one of: table, json, csv",
        "unknown output format should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--event-rate-limit", "0"},
        "--event-rate-limit must be a positive integer",
        "zero event rate limit should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--event-queue-capacity", "-1"},
        "--event-queue-capacity must be a positive integer",
        "negative event queue capacity should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--flip-flop-window", "many"},
        "--flip-flop-window must be a positive integer",
        "non-integer flip-flop window should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--reappearance-threshold", "0"},
        "--reappearance-threshold must be a positive integer",
        "zero reappearance threshold should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--local-net", "192.168.1.0"},
        "--local-net requires a valid IPv4 CIDR value",
        "invalid local CIDR should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--ignore-net", "10.0.0.0/33"},
        "--ignore-net requires a valid IPv4 CIDR value",
        "invalid ignored CIDR should be rejected");
}

void rejectsRemovedLiveFlags()
{
    expectErrorContains(
        {"--interface", "eth0", "--duration", "60"},
        "--duration has been removed",
        "removed --duration should be rejected");
    expectErrorContains(
        {"--interface", "eth0", "--live"},
        "--live is no longer required",
        "removed --live should be rejected");
    expectErrorContains(
        {"--interface", "eth0", "--idle-timeout", "30"},
        "--idle-timeout has been removed",
        "removed --idle-timeout should be rejected");
    expectErrorContains(
        {"--interface", "eth0", "--max-assets", "10"},
        "--max-assets has been removed",
        "removed --max-assets should be rejected");
}

void rejectsRemovedDatabaseAndEventFlags()
{
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--db-url", "postgresql://example"},
        "--db-url has been removed",
        "removed --db-url should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--events", "stdout"},
        "--events has been removed",
        "removed --events should be rejected");
    expectErrorContains(
        {"--pcap", "samples/arp.pcap", "--events-json", "logs/events.ndjson"},
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
}

void usageShowsSimplifiedForms()
{
    const auto usage = usageText("asset-discovery");
    expect(usage.find("--interface <name> [--config <file>|--profile <name>]") != std::string::npos,
        "usage should show implicit live form");
    expect(usage.find("Common options:") != std::string::npos,
        "usage should show common options section");
    expect(usage.find("Advanced overrides:") != std::string::npos,
        "usage should show advanced overrides section");
    expect(usage.find("--version") != std::string::npos,
        "usage should show version option");
    expect(usage.find("--duration") == std::string::npos,
        "usage should not mention --duration");
    expect(usage.find("--events-json") == std::string::npos,
        "usage should not mention removed event sink flags");
    expect(usage.find("ASSET_DISCOVERY_EVENTS_JSON") != std::string::npos,
        "usage should document event path environment override");
}

} // namespace

int main()
{
    parsesPcapMode();
    parsesImplicitLiveMode();
    parsesRetainedLiveControls();
    defersSourceSelectionValidation();
    parsesConfigProfileAndVersion();
    rejectsConfigProfileConflict();
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
