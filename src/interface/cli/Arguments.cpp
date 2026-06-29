#include "interface/cli/Arguments.hpp"

#include <charconv>
#include <sstream>

namespace asset_discovery::cli {
namespace {

std::optional<int> parsePositiveInteger(const std::string& value)
{
    int parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end || parsed <= 0) {
        return std::nullopt;
    }
    return parsed;
}

bool needsValue(const std::string& option, std::size_t index, std::size_t size)
{
    return option.rfind("--", 0) == 0 && index + 1 >= size;
}

} // namespace

ParseResult parseArguments(const std::vector<std::string>& args)
{
    Options options;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        if (arg == "--help" || arg == "-h") {
            options.helpRequested = true;
            return {options, std::nullopt};
        }

        if (arg == "--pcap") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--pcap requires a file path"};
            }
            options.pcapPath = args[++i];
            continue;
        }

        if (arg == "--interface") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--interface requires an interface name"};
            }
            options.interfaceName = args[++i];
            continue;
        }

        if (arg == "--duration") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--duration requires a positive number of seconds"};
            }
            const auto parsed = parsePositiveInteger(args[++i]);
            if (!parsed.has_value()) {
                return {options, "--duration must be a positive integer"};
            }
            options.durationSeconds = *parsed;
            continue;
        }

        if (arg == "--filter") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--filter requires a BPF expression"};
            }
            const auto value = args[++i];
            if (value.empty()) {
                return {options, "--filter cannot be empty"};
            }
            options.packetFilter = value;
            continue;
        }

        if (arg == "--output") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--output requires one of: table, json, csv"};
            }
            const auto value = args[++i];
            if (value == "table") {
                options.outputFormat = OutputFormat::Table;
            } else if (value == "json") {
                options.outputFormat = OutputFormat::Json;
            } else if (value == "csv") {
                options.outputFormat = OutputFormat::Csv;
            } else {
                return {options, "output format '" + value + "' is not supported; expected one of: table, json, csv"};
            }
            continue;
        }

        if (arg == "--db-url") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--db-url requires a PostgreSQL connection string"};
            }
            const auto value = args[++i];
            if (value.empty()) {
                return {options, "--db-url cannot be empty; use DATABASE_URL in .env or pass a valid connection string"};
            }
            options.databaseUrl = value;
            continue;
        }

        return {options, "unknown argument '" + arg + "'"};
    }

    if (options.pcapPath.has_value() == options.interfaceName.has_value()) {
        return {options, "provide exactly one input source: --pcap <file> or --interface <name>"};
    }

    if (options.interfaceName.has_value() && !options.durationSeconds.has_value()) {
        return {options, "--interface requires --duration <seconds>"};
    }

    return {options, std::nullopt};
}

std::string usageText(const std::string& executableName)
{
    std::ostringstream output;
    output << "Usage:\n"
           << "  " << executableName << " --pcap <file> [--filter <bpf>] [--output table|json|csv] [--db-url <url>]\n"
           << "  " << executableName << " --interface <name> --duration <seconds> [--filter <bpf>] [--output table|json|csv] [--db-url <url>]\n"
           << "\nOptions:\n"
           << "  --pcap <file>              Read packets from a PCAP file.\n"
           << "  --interface <name>         Capture packets from a live interface.\n"
           << "  --duration <seconds>       Capture duration; required with --interface.\n"
           << "  --filter <bpf>             Filter packets with a BPF expression, for example: arp or udp port 67 or udp port 68.\n"
           << "  --output table|json|csv    Output format. Defaults to table.\n"
           << "  --db-url <url>             Write assets to PostgreSQL with the psql client.\n"
           << "  -h, --help                 Show this help text.\n";
    return output.str();
}

std::string outputFormatName(OutputFormat format)
{
    switch (format) {
    case OutputFormat::Table:
        return "table";
    case OutputFormat::Json:
        return "json";
    case OutputFormat::Csv:
        return "csv";
    }
    return "table";
}

} // namespace asset_discovery::cli
