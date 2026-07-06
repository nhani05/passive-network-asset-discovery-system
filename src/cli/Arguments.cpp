#include "pnad/cli/Arguments.hpp"

#include "pnad/constants/CliConstants.hpp"
#include "pnad/constants/CaptureConstants.hpp"

#include <sstream>

#ifndef ASSET_DISCOVERY_VERSION
#define ASSET_DISCOVERY_VERSION "unknown"
#endif

namespace asset_discovery::cli {
namespace {

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
        return "--duration has been removed; capture uses PCAP files only";
    }
    if (isOptionOrAssignment(argument, "--live")) {
        return "--live has been removed; use --pcap <file>";
    }
    if (isOptionOrAssignment(argument, "--interface")) {
        return "--interface has been removed; use --pcap <file>";
    }
    if (isOptionOrAssignment(argument, "--capture-backend")) {
        return "--capture-backend has been removed; capture uses PCAP files only";
    }
    if (isOptionOrAssignment(argument, "--config")) {
        return "--config has been removed; configs/default.yaml is loaded automatically";
    }
    if (isOptionOrAssignment(argument, "--profile")) {
        return "--profile has been removed; configs/default.yaml is the only YAML config";
    }
    if (isOptionOrAssignment(argument, "--idle-timeout")) {
        return "--idle-timeout has been removed; live capture no longer stops on idle timeout";
    }
    if (isOptionOrAssignment(argument, "--max-assets")) {
        return "--max-assets has been removed; live capture no longer stops after an asset count";
    }
    if (isOptionOrAssignment(argument, "--db-url")) {
        return "--db-url has been removed; configure SQLite with --sqlite or SQLITE_DATABASE_PATH";
    }
    if (isOptionOrAssignment(argument, "--events")) {
        return "--events has been removed; realtime stdout events are enabled by default";
    }
    if (isOptionOrAssignment(argument, "--events-json")) {
        return "--events-json has been removed; event file output is no longer supported";
    }
    if (isOptionOrAssignment(argument, "--syslog")) {
        return "--syslog has been removed; syslog event output is no longer supported";
    }
    if (isOptionOrAssignment(argument, "--events-db")) {
        return "--events-db has been removed; database writes persist assets only";
    }
    if (isOptionOrAssignment(argument, "--event-rate-limit")) {
        return "--event-rate-limit has been removed; only new_asset events are emitted";
    }
    if (isOptionOrAssignment(argument, "--event-queue-capacity")) {
        return "--event-queue-capacity has been removed; PCAP analysis uses the built-in event path";
    }
    if (isOptionOrAssignment(argument, "--flip-flop-window")) {
        return "--flip-flop-window has been removed; database last_seen tracks asset updates";
    }
    if (isOptionOrAssignment(argument, "--reappearance-threshold")) {
        return "--reappearance-threshold has been removed; database last_seen tracks asset updates";
    }
    if (isOptionOrAssignment(argument, "--local-net")) {
        return "--local-net has been removed; non-local source events are no longer emitted";
    }
    if (isOptionOrAssignment(argument, "--ignore-net")) {
        return "--ignore-net has been removed; non-local source events are no longer emitted";
    }
    return std::nullopt;
}

} // namespace

ParseResult parseArguments(const std::vector<std::string>& args)
{
    Options options;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        if (arg == constants::cli::HelpOption || arg == constants::cli::ShortHelpOption) {
            options.helpRequested = true;
            return {options, std::nullopt};
        }

        if (arg == constants::cli::VersionOption) {
            options.versionRequested = true;
            return {options, std::nullopt};
        }

        if (const auto error = removedOptionError(arg); error.has_value()) {
            return {options, *error};
        }

        if (arg == constants::cli::PcapOption) {
            if (needsValue(arg, i, args.size())) {
                return {options, "--pcap requires a file path"};
            }
            options.pcapPath = args[++i];
            continue;
        }

        if (arg == constants::cli::FilterOption) {
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

        if (arg == constants::cli::SqliteOption) {
            if (needsValue(arg, i, args.size())) {
                return {options, "--sqlite requires a database file path"};
            }
            options.sqlitePath = args[++i];
            continue;
        }

        if (arg == constants::cli::OutputOption) {
            if (needsValue(arg, i, args.size())) {
                return {options, "--output requires one of: table, json, csv"};
            }
            const auto value = args[++i];
            if (value == constants::cli::OutputTable) {
                options.outputFormat = OutputFormat::Table;
            } else if (value == constants::cli::OutputJson) {
                options.outputFormat = OutputFormat::Json;
            } else if (value == constants::cli::OutputCsv) {
                options.outputFormat = OutputFormat::Csv;
            } else {
                return {options, "output format '" + value + "' is not supported; expected one of: table, json, csv"};
            }
            options.outputFormatProvided = true;
            continue;
        }

        return {options, "unknown argument '" + arg + "'"};
    }

    if (options.pcapPath.has_value()) {
        options.captureMode = CaptureMode::PcapOffline;
    }

    return {options, std::nullopt};
}

std::string usageText(const std::string& executableName)
{
    std::ostringstream output;
    output << "Usage:\n"
           << "  " << executableName << " --pcap <file> [--filter <bpf>] [--sqlite <file>] [--output table|json|csv]\n"
           << "  " << executableName << " --version\n"
           << "\nCommon options:\n"
           << "  --pcap <file>              Read packets from a PCAP file.\n"
           << "  --filter <bpf>             Filter packets with a BPF expression, for example: "
           << constants::capture::DefaultPacketFilter << ".\n"
           << "  --sqlite <file>            Save assets in a local SQLite database file.\n"
           << "  --output table|json|csv    Output format. Defaults to json.\n"
           << "  --version                  Show version information.\n"
           << "\nDefault outputs:\n"
           << "  New asset events are written to stdout. SQLite persists asset inventory only.\n"
           << "\n"
           << "  -h, --help                 Show this help text.\n";
    return output.str();
}

std::string versionText()
{
    return std::string("asset-discovery ") + ASSET_DISCOVERY_VERSION + "\n";
}

std::string outputFormatName(OutputFormat format)
{
    switch (format) {
    case OutputFormat::Table:
        return constants::cli::OutputTable;
    case OutputFormat::Json:
        return constants::cli::OutputJson;
    case OutputFormat::Csv:
        return constants::cli::OutputCsv;
    }
    return constants::cli::OutputTable;
}

} // namespace asset_discovery::cli
