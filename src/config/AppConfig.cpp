#include "pnad/config/AppConfig.hpp"

#include "pnad/constants/CaptureConstants.hpp"
#include "pnad/constants/CliConstants.hpp"
#include "pnad/constants/ConfigConstants.hpp"

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

std::optional<cli::OutputFormat> parseOutputFormat(const std::string& value)
{
    if (value == constants::cli::OutputTable) {
        return cli::OutputFormat::Table;
    }
    if (value == constants::cli::OutputJson) {
        return cli::OutputFormat::Json;
    }
    if (value == constants::cli::OutputCsv) {
        return cli::OutputFormat::Csv;
    }
    return std::nullopt;
}

std::string location(const std::string& path, int lineNumber)
{
    return path + ":" + std::to_string(lineNumber) + ": ";
}

std::optional<std::string> parseOutputKey(
    ConfigPatch& patch,
    const std::string& path,
    int lineNumber,
    const std::string& key,
    const std::string& value)
{
    if (key != constants::config::OutputFormatKey) {
        return location(path, lineNumber) + "unknown output key '" + key + "'";
    }
    const auto format = parseOutputFormat(unquoteScalar(value));
    if (!format.has_value()) {
        return location(path, lineNumber) + "output.format must be one of: table, json, csv";
    }
    patch.outputFormat = *format;
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
    if (section == constants::config::OutputSection) {
        return parseOutputKey(patch, path, lineNumber, key, value);
    }
    (void)value;
    return location(path, lineNumber) + "unknown section '" + section + "'";
}

} // namespace

AppConfig builtInDefaults()
{
    AppConfig config;
    config.capture.packetFilter = constants::capture::DefaultPacketFilter;
    return config;
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
            if (section == constants::config::CaptureSection
                || section == constants::config::DatabaseSection
                || section == constants::config::EventsSection
                || section == constants::config::NetworkSection) {
                result.error = location(path, lineNumber) + "section '" + section + "' is no longer supported";
                return result;
            }
            if (section != constants::config::OutputSection) {
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
            result.error = location(path, lineNumber) + "list values are no longer supported";
            return result;
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

ConfigPatch patchFromCliOptions(const cli::Options& options)
{
    ConfigPatch patch;
    patch.pcapPath = options.pcapPath;
    patch.packetFilter = options.packetFilter;
    if (options.broadIpv4Enrichment && !patch.packetFilter.has_value()) {
        patch.packetFilter = constants::capture::BroadIpv4EnrichmentPacketFilter;
    }
    if (options.outputFormatProvided) {
        patch.outputFormat = options.outputFormat;
    }
    if (options.sqlitePath.has_value()) {
        patch.sqlitePath = options.sqlitePath;
    }
    return patch;
}

ConfigPatch patchFromEnvironment(const RuntimeEnvironment& environment)
{
    ConfigPatch patch;
    if (environment.sqlitePath.has_value()) {
        patch.sqlitePath = environment.sqlitePath;
    }
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
    if (patch.sqlitePath.has_value()) {
        config.database.sqlitePath = *patch.sqlitePath;
    }
}

std::optional<std::string> validateConfig(const AppConfig& config)
{
    if (!config.capture.pcapPath.has_value()) {
        return "provide input source: --pcap <file>";
    }
    if (config.capture.interfaceName.has_value()) {
        return "--interface has been removed; use --pcap <file>";
    }
    if (config.capture.packetFilter.has_value() && config.capture.packetFilter->empty()) {
        return "--filter must not be empty";
    }
    if (!config.database.sqlitePath.has_value()) {
        return "SQLite configuration is required; set SQLITE_DATABASE_PATH or --sqlite";
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

    if (buildOptions.loadDefaultConfig && std::filesystem::exists(buildOptions.defaultConfigPath)) {
        const auto defaultPatch = loadConfigFile(buildOptions.defaultConfigPath);
        if (defaultPatch.error.has_value()) {
            result.error = *defaultPatch.error;
            return result;
        }
        applyPatch(result.config, defaultPatch.patch);
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
    (void)config;
    return cli::CaptureMode::PcapOffline;
}

} // namespace asset_discovery::config
