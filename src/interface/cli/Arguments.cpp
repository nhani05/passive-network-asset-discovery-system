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

        if (arg == "--live") {
            options.captureMode = CaptureMode::LiveInfinite;
            continue;
        }

        if (arg == "--idle-timeout") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--idle-timeout requires a positive number of seconds"};
            }
            const auto parsed = parsePositiveInteger(args[++i]);
            if (!parsed.has_value()) {
                return {options, "--idle-timeout must be a positive integer"};
            }
            options.idleTimeoutSeconds = *parsed;
            continue;
        }

        if (arg == "--max-assets") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--max-assets requires a positive asset count"};
            }
            const auto parsed = parsePositiveInteger(args[++i]);
            if (!parsed.has_value()) {
                return {options, "--max-assets must be a positive integer"};
            }
            options.maxAssets = *parsed;
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

    const bool liveRequested = options.captureMode == CaptureMode::LiveInfinite;

    if (options.pcapPath.has_value()) {
        if (options.durationSeconds.has_value()) {
            return {options, "--duration is only valid with --interface <name>"};
        }
        if (liveRequested) {
            return {options, "--live is only valid with --interface <name>"};
        }
        if (options.idleTimeoutSeconds.has_value()) {
            return {options, "--idle-timeout is only valid with --interface <name> --live"};
        }
        if (options.maxAssets.has_value()) {
            return {options, "--max-assets is only valid with --interface <name> --live"};
        }
        if (options.captureBackend != capture::CaptureBackendSelection::Auto) {
            return {options, "--capture-backend is only valid with --interface capture"};
        }
        options.captureMode = CaptureMode::PcapOffline;
        return {options, std::nullopt};
    }

    if (options.durationSeconds.has_value() && liveRequested) {
        return {options, "choose either --duration <seconds> or --live with --interface, not both"};
    }

    if (!options.durationSeconds.has_value() && !liveRequested) {
        return {options, "--interface requires either --duration <seconds> or --live"};
    }

    if (!liveRequested) {
        if (options.idleTimeoutSeconds.has_value()) {
            return {options, "--idle-timeout is only valid with --interface <name> --live"};
        }
        if (options.maxAssets.has_value()) {
            return {options, "--max-assets is only valid with --interface <name> --live"};
        }
        options.captureMode = CaptureMode::LiveTimed;
    } else {
        options.captureMode = CaptureMode::LiveInfinite;
    }

    return {options, std::nullopt};
}

std::string usageText(const std::string& executableName)
{
    std::ostringstream output;
    output << "Usage:\n"
           << "  " << executableName << " --pcap <file> [--filter <bpf>] [--output table|json|csv] [--db-url <url>]\n"
           << "  " << executableName << " --interface <name> --duration <seconds> [--filter <bpf>] [--capture-backend auto|pcap|af-packet] [--output table|json|csv] [--db-url <url>]\n"
           << "  " << executableName << " --interface <name> --live [--idle-timeout <seconds>] [--max-assets <count>] [--filter <bpf>] [--capture-backend auto|pcap|af-packet] [--output table|json|csv] [--db-url <url>]\n"
           << "\nOptions:\n"
           << "  --pcap <file>              Read packets from a PCAP file.\n"
           << "  --interface <name>         Capture packets from a live interface.\n"
           << "  --duration <seconds>       Live timed capture duration.\n"
           << "  --live                     Capture until stopped by Ctrl+C or a live stop condition.\n"
           << "  --idle-timeout <seconds>   In --live mode, stop after no accepted packet is seen for this many seconds.\n"
           << "  --max-assets <count>       In --live mode, stop after discovering this many assets.\n"
           << "  --filter <bpf>             Filter packets with a BPF expression, for example: arp or udp port 67 or udp port 68.\n"
           << "  --capture-backend <name>   Live capture backend: auto, pcap, or af-packet. Defaults to auto.\n"
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
