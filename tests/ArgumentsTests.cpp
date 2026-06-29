#include "interface/cli/Arguments.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

using asset_discovery::cli::parseArguments;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void rejectsEmptyDatabaseUrl()
{
    const std::vector<std::string> args = {
        "--pcap",
        "samples/arp.pcap",
        "--db-url",
        "",
    };

    const auto result = parseArguments(args);
    expect(result.error.has_value(), "empty --db-url should be rejected");
    if (result.error.has_value()) {
        expect(result.error->find("--db-url cannot be empty") != std::string::npos,
            "error should explain that --db-url cannot be empty");
    }
}

void parsesValidDatabaseUrl()
{
    const std::vector<std::string> args = {
        "--pcap",
        "samples/arp.pcap",
        "--db-url",
        "postgresql://postgres:123456@localhost:5432/asset_discovery",
    };

    const auto result = parseArguments(args);
    expect(!result.error.has_value(), "valid --db-url should be accepted");
    expect(result.options.databaseUrl.has_value(), "valid --db-url should be stored");
}

void parsesPacketFilter()
{
    const std::vector<std::string> args = {
        "--interface",
        "eth0",
        "--duration",
        "60",
        "--filter",
        "arp or udp port 67 or udp port 68",
    };

    const auto result = parseArguments(args);
    expect(!result.error.has_value(), "valid --filter should be accepted");
    expect(result.options.packetFilter.has_value(), "valid --filter should be stored");
    expect(result.options.captureMode == asset_discovery::cli::CaptureMode::LiveTimed,
        "timed live capture mode should be selected");
    expect(result.options.durationSeconds == 60, "duration should be stored");
}

void rejectsEmptyPacketFilter()
{
    const std::vector<std::string> args = {
        "--pcap",
        "samples/arp.pcap",
        "--filter",
        "",
    };

    const auto result = parseArguments(args);
    expect(result.error.has_value(), "empty --filter should be rejected");
    if (result.error.has_value()) {
        expect(result.error->find("--filter cannot be empty") != std::string::npos,
            "error should explain that --filter cannot be empty");
    }
}

void parsesPcapMode()
{
    const std::vector<std::string> args = {
        "--pcap",
        "samples/arp.pcap",
    };

    const auto result = parseArguments(args);
    expect(!result.error.has_value(), "valid --pcap mode should be accepted");
    expect(result.options.captureMode == asset_discovery::cli::CaptureMode::PcapOffline,
        "pcap capture mode should be selected");
}

void parsesLiveInfiniteMode()
{
    const std::vector<std::string> args = {
        "--interface",
        "eth0",
        "--live",
        "--idle-timeout",
        "30",
        "--max-assets",
        "10",
    };

    const auto result = parseArguments(args);
    expect(!result.error.has_value(), "valid --live mode should be accepted");
    expect(result.options.captureMode == asset_discovery::cli::CaptureMode::LiveInfinite,
        "live infinite capture mode should be selected");
    expect(result.options.idleTimeoutSeconds == 30, "idle timeout should be stored");
    expect(result.options.maxAssets == 10, "max assets should be stored");
}

void rejectsInterfaceWithoutLiveControl()
{
    const std::vector<std::string> args = {
        "--interface",
        "eth0",
    };

    const auto result = parseArguments(args);
    expect(result.error.has_value(), "--interface without --duration or --live should be rejected");
    if (result.error.has_value()) {
        expect(result.error->find("--duration <seconds> or --live") != std::string::npos,
            "error should explain live control alternatives");
    }
}

void rejectsDurationWithLive()
{
    const std::vector<std::string> args = {
        "--interface",
        "eth0",
        "--duration",
        "60",
        "--live",
    };

    const auto result = parseArguments(args);
    expect(result.error.has_value(), "--duration and --live should conflict");
    if (result.error.has_value()) {
        expect(result.error->find("either --duration") != std::string::npos,
            "error should explain duration/live conflict");
    }
}

void rejectsInfiniteControlsWithoutLive()
{
    const std::vector<std::string> args = {
        "--interface",
        "eth0",
        "--duration",
        "60",
        "--idle-timeout",
        "30",
    };

    const auto result = parseArguments(args);
    expect(result.error.has_value(), "--idle-timeout without --live should be rejected");
    if (result.error.has_value()) {
        expect(result.error->find("--idle-timeout is only valid") != std::string::npos,
            "error should explain idle-timeout scope");
    }
}

void rejectsInvalidLiveControlValues()
{
    const auto invalidDuration = parseArguments({"--interface", "eth0", "--duration", "0"});
    expect(invalidDuration.error.has_value(), "zero duration should be rejected");

    const auto invalidIdleTimeout = parseArguments({"--interface", "eth0", "--live", "--idle-timeout", "-1"});
    expect(invalidIdleTimeout.error.has_value(), "negative idle timeout should be rejected");

    const auto invalidMaxAssets = parseArguments({"--interface", "eth0", "--live", "--max-assets", "many"});
    expect(invalidMaxAssets.error.has_value(), "non-integer max assets should be rejected");
}

void rejectsPcapWithLiveControl()
{
    const auto pcapWithDuration = parseArguments({"--pcap", "samples/arp.pcap", "--duration", "60"});
    expect(pcapWithDuration.error.has_value(), "--pcap with --duration should be rejected");

    const auto pcapWithLive = parseArguments({"--pcap", "samples/arp.pcap", "--live"});
    expect(pcapWithLive.error.has_value(), "--pcap with --live should be rejected");
}

} // namespace

int main()
{
    rejectsEmptyDatabaseUrl();
    parsesValidDatabaseUrl();
    parsesPacketFilter();
    rejectsEmptyPacketFilter();
    parsesPcapMode();
    parsesLiveInfiniteMode();
    rejectsInterfaceWithoutLiveControl();
    rejectsDurationWithLive();
    rejectsInfiniteControlsWithoutLive();
    rejectsInvalidLiveControlValues();
    rejectsPcapWithLiveControl();

    if (failures > 0) {
        std::cerr << failures << " argument test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
