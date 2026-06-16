#include "cli/Arguments.hpp"

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

        if (arg == "--output") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--output requires one of: table, json"};
            }
            const auto value = args[++i];
            if (value == "table") {
                options.outputFormat = OutputFormat::Table;
            } else if (value == "json") {
                options.outputFormat = OutputFormat::Json;
            } else {
                return {options, "unsupported output format '" + value + "'; expected one of: table, json"};
            }
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
           << "  " << executableName << " --pcap <file> [--output table|json]\n"
           << "  " << executableName << " --interface <name> --duration <seconds> [--output table|json]\n"
           << "\nOptions:\n"
           << "  --pcap <file>              Read packets from a PCAP file.\n"
           << "  --interface <name>         Capture packets from a live interface.\n"
           << "  --duration <seconds>       Live capture duration; required with --interface.\n"
           << "  --output table|json        Output format. Defaults to table.\n"
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
    }
    return "table";
}

} // namespace asset_discovery::cli
