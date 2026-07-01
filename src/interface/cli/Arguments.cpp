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

bool isOptionOrAssignment(const std::string& argument, const std::string& option)
{
    return argument == option || argument.rfind(option + "=", 0) == 0;
}

std::optional<std::string> removedOptionError(const std::string& argument)
{
    if (isOptionOrAssignment(argument, "--duration")) {
        return "--duration has been removed; live capture now runs until interrupted";
    }
    if (isOptionOrAssignment(argument, "--live")) {
        return "--live is no longer required; --interface starts live capture";
    }
    if (isOptionOrAssignment(argument, "--idle-timeout")) {
        return "--idle-timeout has been removed; live capture no longer stops on idle timeout";
    }
    if (isOptionOrAssignment(argument, "--max-assets")) {
        return "--max-assets has been removed; live capture no longer stops after an asset count";
    }
    if (isOptionOrAssignment(argument, "--db-url")) {
        return "--db-url has been removed; configure PostgreSQL with .env, DATABASE_URL, PG*, or DB_* environment variables";
    }
    if (isOptionOrAssignment(argument, "--events")) {
        return "--events has been removed; realtime stdout events are enabled by default";
    }
    if (isOptionOrAssignment(argument, "--events-json")) {
        return "--events-json has been removed; NDJSON event output is enabled by default; set ASSET_DISCOVERY_EVENTS_JSON to change the path";
    }
    if (isOptionOrAssignment(argument, "--syslog")) {
        return "--syslog has been removed; syslog events are auto-enabled when supported";
    }
    if (isOptionOrAssignment(argument, "--events-db")) {
        return "--events-db has been removed; database event writes are enabled when database environment exists";
    }
    return std::nullopt;
}

std::optional<capture::CaptureBackendSelection> parseBackendSelection(const std::string& value)
{
    if (value == "auto") {
        return capture::CaptureBackendSelection::Auto;
    }
    if (value == "pcap") {
        return capture::CaptureBackendSelection::Pcap;
    }
    if (value == "af-packet") {
        return capture::CaptureBackendSelection::AfPacket;
    }
    return std::nullopt;
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

        if (const auto error = removedOptionError(arg); error.has_value()) {
            return {options, *error};
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

        if (arg == "--capture-backend") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--capture-backend requires one of: auto, pcap, af-packet"};
            }
            const auto value = args[++i];
            const auto backend = parseBackendSelection(value);
            if (!backend.has_value()) {
                return {options, "capture backend '" + value + "' is not supported; expected one of: auto, pcap, af-packet"};
            }
            options.captureBackend = *backend;
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

        if (arg == "--event-rate-limit") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--event-rate-limit requires a positive number of seconds"};
            }
            const auto parsed = parsePositiveInteger(args[++i]);
            if (!parsed.has_value()) {
                return {options, "--event-rate-limit must be a positive integer"};
            }
            options.eventRateLimitSeconds = *parsed;
            continue;
        }

        if (arg == "--event-queue-capacity") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--event-queue-capacity requires a positive event count"};
            }
            const auto parsed = parsePositiveInteger(args[++i]);
            if (!parsed.has_value()) {
                return {options, "--event-queue-capacity must be a positive integer"};
            }
            options.eventQueueCapacity = *parsed;
            continue;
        }

        if (arg == "--flip-flop-window") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--flip-flop-window requires a positive number of seconds"};
            }
            const auto parsed = parsePositiveInteger(args[++i]);
            if (!parsed.has_value()) {
                return {options, "--flip-flop-window must be a positive integer"};
            }
            options.flipFlopWindowSeconds = *parsed;
            continue;
        }

        if (arg == "--reappearance-threshold") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--reappearance-threshold requires a positive number of seconds"};
            }
            const auto parsed = parsePositiveInteger(args[++i]);
            if (!parsed.has_value()) {
                return {options, "--reappearance-threshold must be a positive integer"};
            }
            options.reappearanceThresholdSeconds = *parsed;
            continue;
        }

        if (arg == "--local-net") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--local-net requires an IPv4 CIDR value"};
            }
            const auto value = args[++i];
            const auto network = asset::parseIpv4Network(value);
            if (!network.has_value()) {
                return {options, "--local-net requires a valid IPv4 CIDR value"};
            }
            options.localNetworks.push_back(*network);
            continue;
        }

        if (arg == "--ignore-net") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--ignore-net requires an IPv4 CIDR value"};
            }
            const auto value = args[++i];
            const auto network = asset::parseIpv4Network(value);
            if (!network.has_value()) {
                return {options, "--ignore-net requires a valid IPv4 CIDR value"};
            }
            options.ignoredNetworks.push_back(*network);
            continue;
        }

        return {options, "unknown argument '" + arg + "'"};
    }

    if (options.pcapPath.has_value() == options.interfaceName.has_value()) {
        return {options, "provide exactly one input source: --pcap <file> or --interface <name>"};
    }

    if (options.pcapPath.has_value()) {
        if (options.captureBackend != capture::CaptureBackendSelection::Auto) {
            return {options, "--capture-backend is only valid with --interface capture"};
        }
        options.captureMode = CaptureMode::PcapOffline;
        return {options, std::nullopt};
    }

    options.captureMode = CaptureMode::Live;

    return {options, std::nullopt};
}

std::string usageText(const std::string& executableName)
{
    std::ostringstream output;
    output << "Usage:\n"
           << "  " << executableName << " --pcap <file> [--filter <bpf>] [--output table|json|csv]\n"
           << "  " << executableName << " --interface <name> [--filter <bpf>] [--capture-backend auto|pcap|af-packet] [--output table|json|csv]\n"
           << "\nOptions:\n"
           << "  --pcap <file>              Read packets from a PCAP file.\n"
           << "  --interface <name>         Capture packets from a live interface until SIGINT or SIGTERM.\n"
           << "  --filter <bpf>             Filter packets with a BPF expression, for example: arp or udp port 67 or udp port 68.\n"
           << "  --capture-backend <name>   Live capture backend: auto, pcap, or af-packet. Defaults to auto.\n"
           << "  --output table|json|csv    Output format. Defaults to table.\n"
           << "  --event-rate-limit <sec>   Suppress duplicate low-value events within this window.\n"
           << "  --event-queue-capacity <n> Live event queue capacity. Defaults to 1024.\n"
           << "  --flip-flop-window <sec>   Window for detecting IP/MAC flip-flop events.\n"
           << "  --reappearance-threshold <sec> Threshold for asset_reappeared events.\n"
           << "  --local-net <cidr>         Local IPv4 network for non-local source detection. Repeatable.\n"
           << "  --ignore-net <cidr>        Ignored IPv4 network for non-local source detection. Repeatable.\n"
           << "\nDefault outputs:\n"
           << "  Realtime events are written to stdout, syslog when supported, and NDJSON at\n"
           << "  ASSET_DISCOVERY_EVENTS_JSON or logs/events.ndjson. PostgreSQL writes use\n"
           << "  .env, DATABASE_URL, PG*, or DB_* environment variables when present.\n"
           << "\n"
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
