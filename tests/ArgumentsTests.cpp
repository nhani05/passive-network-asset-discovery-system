#include "cli/Arguments.hpp"

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
        expect(result.error->find("--db-url không được rỗng") != std::string::npos,
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
        expect(result.error->find("--filter không được rỗng") != std::string::npos,
            "error should explain that --filter cannot be empty");
    }
}

} // namespace

int main()
{
    rejectsEmptyDatabaseUrl();
    parsesValidDatabaseUrl();
    parsesPacketFilter();
    rejectsEmptyPacketFilter();

    if (failures > 0) {
        std::cerr << failures << " argument test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
