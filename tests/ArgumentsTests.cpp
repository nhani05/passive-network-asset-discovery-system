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

} // namespace

int main()
{
    rejectsEmptyDatabaseUrl();
    parsesValidDatabaseUrl();

    if (failures > 0) {
        std::cerr << failures << " argument test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
