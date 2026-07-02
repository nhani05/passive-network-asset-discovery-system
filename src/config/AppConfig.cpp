#include "pnad/config/AppConfig.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace asset_discovery::config {
namespace {

std::string trim(std::string value)
{
    const auto isSpace = [](unsigned char character) {
        return std::isspace(character) != 0;
    };

    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string stripComment(const std::string& line)
{
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
        if (character == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
        } else if (character == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
        } else if (character == '#' && !inSingleQuote && !inDoubleQuote) {
            return line.substr(0, index);
        }
    }
    return line;
}

std::size_t leadingSpaces(const std::string& line)
{
    std::size_t count = 0;
    while (count < line.size() && line[count] == ' ') {
        ++count;
    }
    return count;
}

bool hasTabIndent(const std::string& line)
{
    for (const char character : line) {
        if (character == '\t') {
            return true;
        }
        if (character != ' ') {
            return false;
        }
    }
    return false;
}

std::string unquoteScalar(std::string value)
{
    value = trim(std::move(value));
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

std::optional<std::int64_t> parseInteger(const std::string& value)
{
    std::int64_t parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<capture::CaptureBackendSelection> parseBackend(const std::string& value)
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

std::optional<cli::OutputFormat> parseOutputFormat(const std::string& value)
{
    if (value == "table") {
        return cli::OutputFormat::Table;
    }
    if (value == "json") {
        return cli::OutputFormat::Json;
    }
    if (value == "csv") {
        return cli::OutputFormat::Csv;
    }
    return std::nullopt;
}

std::string location(const std::string& path, int lineNumber)
{
    return path + ":" + std::to_string(lineNumber) + ": ";
}

std::optional<std::string> parseNetworkListValue(
    ConfigPatch& patch,
    const std::string& path,
    int lineNumber,
    const std::string& key,
    const std::string& value)
{
    const auto network = asset::parseIpv4Network(value);
    if (!network.has_value()) {
        return location(path, lineNumber) + key + " requires a valid IPv4 CIDR value";
    }
    if (key == "local_nets") {
        if (!patch.localNetworks.has_value()) {
            patch.localNetworks = std::vector<asset::Ipv4Network>{};
        }
        patch.localNetworks->push_back(*network);
    } else {
        if (!patch.ignoredNetworks.has_value()) {
            patch.ignoredNetworks = std::vector<asset::Ipv4Network>{};
        }
        patch.ignoredNetworks->push_back(*network);
    }
    return std::nullopt;
}

std::optional<std::string> parseCaptureKey(
    ConfigPatch& patch,
    const std::string& path,
    int lineNumber,
    const std::string& key,
    const std::string& value)
{
    if (key == "interface" || key == "interface_name" || key == "pcap" || key == "pcap_file") {
        return location(path, lineNumber) + "packet sources must be supplied on the CLI";
    }
    if (key == "filter") {
        patch.packetFilter = unquoteScalar(value);
        return std::nullopt;
    }
    if (key == "backend") {
        const auto backend = parseBackend(unquoteScalar(value));
        if (!backend.has_value()) {
            return location(path, lineNumber) + "capture.backend must be one of: auto, pcap, af-packet";
        }
        patch.backend = *backend;
        return std::nullopt;
    }
    return location(path, lineNumber) + "unknown capture key '" + key + "'";
}

std::optional<std::string> parseOutputKey(
    ConfigPatch& patch,
    const std::string& path,
    int lineNumber,
    const std::string& key,
    const std::string& value)
{
    if (key != "format") {
        return location(path, lineNumber) + "unknown output key '" + key + "'";
    }
    const auto format = parseOutputFormat(unquoteScalar(value));
    if (!format.has_value()) {
        return location(path, lineNumber) + "output.format must be one of: table, json, csv";
    }
    patch.outputFormat = *format;
    return std::nullopt;
}

std::optional<std::string> parseEventsKey(
    ConfigPatch& patch,
    const std::string& path,
    int lineNumber,
    const std::string& key,
    const std::string& value)
{
    const auto parsed = parseInteger(unquoteScalar(value));
    if (!parsed.has_value()) {
        return location(path, lineNumber) + "events." + key + " must be an integer";
    }

    if (key == "rate_limit_sec") {
        patch.eventRateLimitSeconds = *parsed;
    } else if (key == "queue_capacity") {
        patch.eventQueueCapacity = *parsed;
    } else if (key == "flip_flop_window_sec") {
        patch.flipFlopWindowSeconds = *parsed;
    } else if (key == "reappearance_threshold_sec") {
        patch.reappearanceThresholdSeconds = *parsed;
    } else {
        return location(path, lineNumber) + "unknown events key '" + key + "'";
    }
    return std::nullopt;
}

std::optional<std::string> parseScalarKey(
    ConfigPatch& patch,
    const std::string& path,
    int lineNumber,
    const std::string& section,
    const std::string& key,
    const std::string& value)
{
    if (section == "capture") {
        return parseCaptureKey(patch, path, lineNumber, key, value);
    }
    if (section == "output") {
        return parseOutputKey(patch, path, lineNumber, key, value);
    }
    if (section == "events") {
        return parseEventsKey(patch, path, lineNumber, key, value);
    }
    if (section == "network") {
        if (key == "local_nets" || key == "ignore_nets") {
            if (trim(value) == "[]") {
                if (key == "local_nets") {
                    patch.localNetworks = std::vector<asset::Ipv4Network>{};
                } else {
                    patch.ignoredNetworks = std::vector<asset::Ipv4Network>{};
                }
                return std::nullopt;
            }
            return location(path, lineNumber) + "network." + key + " expects a list";
        }
        return location(path, lineNumber) + "unknown network key '" + key + "'";
    }
    return location(path, lineNumber) + "unknown section '" + section + "'";
}

bool validProfileCharacter(char character)
{
    return std::isalnum(static_cast<unsigned char>(character)) != 0
        || character == '_'
        || character == '-';
}

} // namespace

AppConfig builtInDefaults()
{
    return {};
}

PatchResult loadConfigFile(const std::string& path)
{
    PatchResult result;

    std::ifstream input(path);
    if (!input) {
        result.error = "could not open config file '" + path + "'";
        return result;
    }

    std::string section;
    std::string activeListKey;
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        line = stripComment(line);
        if (trim(line).empty()) {
            continue;
        }
        if (hasTabIndent(line)) {
            result.error = location(path, lineNumber) + "tab indentation is not supported";
            return result;
        }

        const auto indent = leadingSpaces(line);
        if (indent != 0 && indent != 2 && indent != 4) {
            result.error = location(path, lineNumber) + "unsupported indentation; use two-space YAML indentation";
            return result;
        }

        const auto content = trim(line);
        if (indent == 0) {
            activeListKey.clear();
            const auto separator = content.find(':');
            if (separator == std::string::npos || separator + 1 != content.size()) {
                result.error = location(path, lineNumber) + "top-level sections must use '<section>:'";
                return result;
            }
            section = content.substr(0, separator);
            if (section == "database") {
                result.error = location(path, lineNumber) + "database connection values must come from .env or process environment variables";
                return result;
            }
            if (section != "capture" && section != "output" && section != "events" && section != "network") {
                result.error = location(path, lineNumber) + "unknown section '" + section + "'";
                return result;
            }
            continue;
        }

        if (section.empty()) {
            result.error = location(path, lineNumber) + "config values must appear under a supported section";
            return result;
        }

        if (indent == 4) {
            if (activeListKey.empty() || content.rfind("- ", 0) != 0) {
                result.error = location(path, lineNumber) + "unsupported nested config value";
                return result;
            }
            const auto value = unquoteScalar(content.substr(2));
            if (value.empty()) {
                result.error = location(path, lineNumber) + "network list values cannot be empty";
                return result;
            }
            if (const auto error = parseNetworkListValue(result.patch, path, lineNumber, activeListKey, value);
                error.has_value()) {
                result.error = *error;
                return result;
            }
            continue;
        }

        activeListKey.clear();
        if (content.rfind("- ", 0) == 0) {
            result.error = location(path, lineNumber) + "list item is not attached to a supported key";
            return result;
        }
        const auto separator = content.find(':');
        if (separator == std::string::npos) {
            result.error = location(path, lineNumber) + "expected '<key>: <value>'";
            return result;
        }

        const auto key = content.substr(0, separator);
        const auto value = trim(content.substr(separator + 1));
        if (section == "network" && (key == "local_nets" || key == "ignore_nets") && value.empty()) {
            activeListKey = key;
            if (key == "local_nets") {
                result.patch.localNetworks = std::vector<asset::Ipv4Network>{};
            } else {
                result.patch.ignoredNetworks = std::vector<asset::Ipv4Network>{};
            }
            continue;
        }
        if (value.empty()) {
            result.error = location(path, lineNumber) + section + "." + key + " requires a value";
            return result;
        }
        if (const auto error = parseScalarKey(result.patch, path, lineNumber, section, key, value);
            error.has_value()) {
            result.error = *error;
            return result;
        }
    }

    return result;
}

std::optional<std::string> resolveProfilePath(
    const std::string& profileName,
    const std::string& profileDirectory)
{
    if (profileName.empty()) {
        return std::nullopt;
    }
    if (!std::all_of(profileName.begin(), profileName.end(), validProfileCharacter)) {
        return std::nullopt;
    }

    const std::filesystem::path profilePath =
        std::filesystem::path(profileDirectory) / (profileName + ".yaml");
    return profilePath.generic_string();
}

ConfigPatch patchFromCliOptions(const cli::Options& options)
{
    ConfigPatch patch;
    patch.pcapPath = options.pcapPath;
    patch.interfaceName = options.interfaceName;
    patch.packetFilter = options.packetFilter;
    if (options.captureBackendProvided) {
        patch.backend = options.captureBackend;
    }
    if (options.outputFormatProvided) {
        patch.outputFormat = options.outputFormat;
    }
    patch.eventRateLimitSeconds = options.eventRateLimitSeconds;
    patch.eventQueueCapacity = options.eventQueueCapacity;
    patch.flipFlopWindowSeconds = options.flipFlopWindowSeconds;
    patch.reappearanceThresholdSeconds = options.reappearanceThresholdSeconds;
    if (!options.localNetworks.empty()) {
        patch.localNetworks = options.localNetworks;
    }
    if (!options.ignoredNetworks.empty()) {
        patch.ignoredNetworks = options.ignoredNetworks;
    }
    return patch;
}

ConfigPatch patchFromEnvironment(const RuntimeEnvironment& environment)
{
    ConfigPatch patch;
    patch.eventNdjsonPath = environment.eventNdjsonPath;
    patch.databaseUrl = environment.databaseUrl;
    patch.databaseConfigured = environment.databaseConfigured;
    return patch;
}

void applyPatch(AppConfig& config, const ConfigPatch& patch)
{
    if (patch.pcapPath.has_value()) {
        config.capture.pcapPath = patch.pcapPath;
    }
    if (patch.interfaceName.has_value()) {
        config.capture.interfaceName = patch.interfaceName;
    }
    if (patch.packetFilter.has_value()) {
        config.capture.packetFilter = patch.packetFilter;
    }
    if (patch.backend.has_value()) {
        config.capture.backend = *patch.backend;
    }
    if (patch.outputFormat.has_value()) {
        config.output.format = *patch.outputFormat;
    }
    if (patch.eventRateLimitSeconds.has_value()) {
        config.events.rateLimitSeconds = *patch.eventRateLimitSeconds;
    }
    if (patch.eventQueueCapacity.has_value()) {
        config.events.queueCapacity = *patch.eventQueueCapacity;
    }
    if (patch.flipFlopWindowSeconds.has_value()) {
        config.events.flipFlopWindowSeconds = *patch.flipFlopWindowSeconds;
    }
    if (patch.reappearanceThresholdSeconds.has_value()) {
        config.events.reappearanceThresholdSeconds = *patch.reappearanceThresholdSeconds;
    }
    if (patch.localNetworks.has_value()) {
        config.network.localNetworks = *patch.localNetworks;
    }
    if (patch.ignoredNetworks.has_value()) {
        config.network.ignoredNetworks = *patch.ignoredNetworks;
    }
    if (patch.eventNdjsonPath.has_value()) {
        config.eventNdjsonPath = *patch.eventNdjsonPath;
    }
    if (patch.databaseUrl.has_value()) {
        config.database.url = *patch.databaseUrl;
    }
    if (patch.databaseConfigured.has_value()) {
        config.database.configured = *patch.databaseConfigured;
    }
}

std::optional<std::string> validateConfig(const AppConfig& config)
{
    if (config.capture.pcapPath.has_value() == config.capture.interfaceName.has_value()) {
        return "provide exactly one input source: --pcap <file> or --interface <name>";
    }
    if (config.capture.pcapPath.has_value()
        && config.capture.backend != capture::CaptureBackendSelection::Auto) {
        return "--capture-backend is only valid with --interface capture";
    }
    if (config.capture.packetFilter.has_value() && config.capture.packetFilter->empty()) {
        return "--filter must not be empty";
    }
    if (config.events.rateLimitSeconds <= 0) {
        return "events.rate_limit_sec must be a positive integer";
    }
    if (config.events.queueCapacity <= 0) {
        return "events.queue_capacity must be a positive integer";
    }
    if (config.events.flipFlopWindowSeconds <= 0) {
        return "events.flip_flop_window_sec must be a positive integer";
    }
    if (config.events.reappearanceThresholdSeconds <= 0) {
        return "events.reappearance_threshold_sec must be a positive integer";
    }
    if (!config.database.configured) {
        return "PostgreSQL configuration is required; set DATABASE_URL or PG*/DB_* values in .env or the process environment";
    }
    return std::nullopt;
}

ConfigResult buildAppConfig(
    const cli::Options& options,
    const RuntimeEnvironment& environment,
    const BuildConfigOptions& buildOptions)
{
    ConfigResult result;
    result.config = builtInDefaults();

    if (options.configPath.has_value() && options.profileName.has_value()) {
        result.error = "--config and --profile cannot be combined";
        return result;
    }

    if (buildOptions.loadDefaultConfig && std::filesystem::exists(buildOptions.defaultConfigPath)) {
        const auto defaultPatch = loadConfigFile(buildOptions.defaultConfigPath);
        if (defaultPatch.error.has_value()) {
            result.error = *defaultPatch.error;
            return result;
        }
        applyPatch(result.config, defaultPatch.patch);
    }

    if (options.configPath.has_value()) {
        const auto explicitPatch = loadConfigFile(*options.configPath);
        if (explicitPatch.error.has_value()) {
            result.error = *explicitPatch.error;
            return result;
        }
        applyPatch(result.config, explicitPatch.patch);
    } else if (options.profileName.has_value()) {
        const auto profilePath = resolveProfilePath(*options.profileName, buildOptions.profileDirectory);
        if (!profilePath.has_value()) {
            result.error = "--profile must contain only letters, digits, underscores, or hyphens";
            return result;
        }
        const auto profilePatch = loadConfigFile(*profilePath);
        if (profilePatch.error.has_value()) {
            result.error = *profilePatch.error;
            return result;
        }
        applyPatch(result.config, profilePatch.patch);
    }

    applyPatch(result.config, patchFromEnvironment(environment));
    applyPatch(result.config, patchFromCliOptions(options));

    if (const auto validationError = validateConfig(result.config); validationError.has_value()) {
        result.error = *validationError;
        return result;
    }

    return result;
}

cli::CaptureMode captureMode(const AppConfig& config)
{
    if (config.capture.pcapPath.has_value()) {
        return cli::CaptureMode::PcapOffline;
    }
    return cli::CaptureMode::Live;
}

} // namespace asset_discovery::config
