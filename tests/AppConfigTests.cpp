#include "pnad/config/AppConfig.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

using asset_discovery::capture::CaptureBackendSelection;
using asset_discovery::config::BuildConfigOptions;
using asset_discovery::config::RuntimeEnvironment;
using asset_discovery::config::buildAppConfig;
using asset_discovery::config::loadConfigFile;
using asset_discovery::cli::OutputFormat;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

std::filesystem::path testRoot()
{
    const auto root = std::filesystem::temp_directory_path()
        / "asset-discovery-app-config-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void writeFile(const std::filesystem::path& path, const std::string& contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
}

RuntimeEnvironment configuredEnvironment(const std::string& eventPath = "custom/events.ndjson")
{
    RuntimeEnvironment environment;
    environment.databaseConfigured = true;
    environment.databaseUrl = std::string{"postgresql://example"};
    environment.eventNdjsonPath = eventPath;
    return environment;
}

void expectLoadErrorContains(
    const std::filesystem::path& path,
    const std::string& expected,
    const std::string& message)
{
    const auto result = loadConfigFile(path.string());
    expect(result.error.has_value(), message);
    if (result.error.has_value()) {
        expect(result.error->find(expected) != std::string::npos,
            "load error should contain: " + expected);
    }
}

void expectBuildErrorContains(
    const asset_discovery::cli::Options& options,
    const RuntimeEnvironment& environment,
    const BuildConfigOptions& buildOptions,
    const std::string& expected,
    const std::string& message)
{
    const auto result = buildAppConfig(options, environment, buildOptions);
    expect(result.error.has_value(), message);
    if (result.error.has_value()) {
        expect(result.error->find(expected) != std::string::npos,
            "build error should contain: " + expected);
    }
}

void loadsSupportedYamlSubset()
{
    const auto root = testRoot();
    const auto path = root / "valid.yaml";
    writeFile(path,
        "# supported config\n"
        "capture:\n"
        "  filter: \"arp or udp port 67 or udp port 68\"\n"
        "  backend: af-packet\n"
        "output:\n"
        "  format: csv\n"
        "events:\n"
        "  rate_limit_sec: 5\n"
        "  queue_capacity: 2048\n"
        "  flip_flop_window_sec: 30\n"
        "  reappearance_threshold_sec: 300\n"
        "network:\n"
        "  local_nets:\n"
        "    - '192.168.1.0/24'\n"
        "  ignore_nets:\n"
        "    - \"169.254.0.0/16\"\n");

    const auto result = loadConfigFile(path.string());
    expect(!result.error.has_value(), "valid YAML subset should load");
    expect(result.patch.packetFilter == "arp or udp port 67 or udp port 68",
        "quoted filter should be parsed");
    expect(result.patch.backend == CaptureBackendSelection::AfPacket,
        "backend should be parsed");
    expect(result.patch.outputFormat == OutputFormat::Csv,
        "output format should be parsed");
    expect(result.patch.eventRateLimitSeconds == 5,
        "rate limit should be parsed");
    expect(result.patch.eventQueueCapacity == 2048,
        "queue capacity should be parsed");
    expect(result.patch.localNetworks.has_value() && result.patch.localNetworks->size() == 1,
        "local network list should be parsed");
    expect(result.patch.ignoredNetworks.has_value() && result.patch.ignoredNetworks->size() == 1,
        "ignored network list should be parsed");
}

void rejectsInvalidYamlAndSensitiveFields()
{
    const auto root = testRoot();

    const auto unknown = root / "unknown.yaml";
    writeFile(unknown, "capture:\n  queue_capcity: 10\n");
    expectLoadErrorContains(unknown, "unknown capture key", "unknown keys should be rejected");

    const auto source = root / "source.yaml";
    writeFile(source, "capture:\n  interface: eth0\n");
    expectLoadErrorContains(source, "packet sources must be supplied on the CLI",
        "source keys should be rejected");

    const auto database = root / "database.yaml";
    writeFile(database, "database:\n  url: postgresql://example\n");
    expectLoadErrorContains(database, "database connection values must come from",
        "database values should be rejected");

    const auto malformedCidr = root / "cidr.yaml";
    writeFile(malformedCidr, "network:\n  ignore_nets:\n    - \"10.0.0.0/33\"\n");
    expectLoadErrorContains(malformedCidr, "valid IPv4 CIDR",
        "malformed CIDR should be rejected");

    const auto tab = root / "tab.yaml";
    writeFile(tab, "capture:\n\tfilter: arp\n");
    expectLoadErrorContains(tab, "tab indentation",
        "tab indentation should be rejected");
}

void mergesPrecedenceDeterministically()
{
    const auto root = testRoot();
    writeFile(root / "configs" / "default.yaml",
        "capture:\n"
        "  filter: \"arp or udp port 67 or udp port 68\"\n"
        "output:\n"
        "  format: json\n"
        "events:\n"
        "  rate_limit_sec: 60\n");
    writeFile(root / "configs" / "json.yaml",
        "capture:\n"
        "  filter: \"udp port 67\"\n"
        "output:\n"
        "  format: json\n"
        "events:\n"
        "  rate_limit_sec: 120\n");

    asset_discovery::cli::Options options;
    options.pcapPath = "samples/arp.pcap";
    options.configPath = (root / "configs" / "json.yaml").string();
    options.packetFilter = "arp";
    options.outputFormat = OutputFormat::Csv;
    options.outputFormatProvided = true;
    options.eventRateLimitSeconds = 5;

    BuildConfigOptions buildOptions;
    buildOptions.defaultConfigPath = (root / "configs" / "default.yaml").string();
    buildOptions.profileDirectory = (root / "configs").string();

    const auto result = buildAppConfig(options, configuredEnvironment("env/events.ndjson"), buildOptions);
    expect(!result.error.has_value(), "merged config should validate");
    expect(result.config.capture.packetFilter == "arp",
        "CLI filter should override explicit config and default config");
    expect(result.config.output.format == OutputFormat::Csv,
        "CLI output should override explicit config and default config");
    expect(result.config.events.rateLimitSeconds == 5,
        "CLI event rate limit should override config");
    expect(result.config.eventNdjsonPath == "env/events.ndjson",
        "environment event path should be applied");
    expect(result.config.database.configured, "database environment should be applied");
}

void builtInOutputDefaultsToJson()
{
    asset_discovery::cli::Options options;
    options.pcapPath = "samples/arp.pcap";

    BuildConfigOptions buildOptions;
    buildOptions.loadDefaultConfig = false;

    const auto result = buildAppConfig(options, configuredEnvironment(), buildOptions);
    expect(!result.error.has_value(), "built-in defaults should validate with configured environment");
    expect(result.config.output.format == OutputFormat::Json,
        "built-in output format should default to json");
}

void loadsProfilesAndHandlesMissingFiles()
{
    const auto root = testRoot();
    writeFile(root / "configs" / "live.yaml",
        "capture:\n"
        "  backend: pcap\n"
        "events:\n"
        "  queue_capacity: 4096\n");

    asset_discovery::cli::Options options;
    options.interfaceName = "eth0";
    options.profileName = "live";

    BuildConfigOptions buildOptions;
    buildOptions.defaultConfigPath = (root / "configs" / "missing-default.yaml").string();
    buildOptions.profileDirectory = (root / "configs").string();

    const auto result = buildAppConfig(options, configuredEnvironment(), buildOptions);
    expect(!result.error.has_value(), "profile should load and missing default should be non-fatal");
    expect(result.config.capture.backend == CaptureBackendSelection::Pcap,
        "profile backend should be loaded");
    expect(result.config.events.queueCapacity == 4096,
        "profile event queue capacity should be loaded");

    options.profileName = "../live";
    expectBuildErrorContains(options, configuredEnvironment(), buildOptions, "--profile must contain",
        "unsafe profile names should be rejected");

    options.profileName = "missing";
    expectBuildErrorContains(options, configuredEnvironment(), buildOptions, "could not open config file",
        "missing explicit profile should be fatal");
}

void validatesMergedConfig()
{
    const auto root = testRoot();
    BuildConfigOptions buildOptions;
    buildOptions.loadDefaultConfig = false;
    buildOptions.profileDirectory = root.string();

    asset_discovery::cli::Options options;
    options.pcapPath = "samples/arp.pcap";

    auto invalid = root / "empty-filter.yaml";
    writeFile(invalid, "capture:\n  filter: \"\"\n");
    options.configPath = invalid.string();
    expectBuildErrorContains(options, configuredEnvironment(), buildOptions, "--filter must not be empty",
        "empty config filter should be rejected after merge");

    invalid = root / "queue.yaml";
    writeFile(invalid, "events:\n  queue_capacity: 0\n");
    options.configPath = invalid.string();
    expectBuildErrorContains(options, configuredEnvironment(), buildOptions, "events.queue_capacity",
        "non-positive queue capacity should be rejected");

    invalid = root / "backend.yaml";
    writeFile(invalid, "capture:\n  backend: af-packet\n");
    options.configPath = invalid.string();
    expectBuildErrorContains(options, configuredEnvironment(), buildOptions, "--capture-backend is only valid",
        "backend selection should remain live-only");

    options = {};
    options.configPath = (root / "missing-source.yaml").string();
    writeFile(*options.configPath, "output:\n  format: table\n");
    expectBuildErrorContains(options, configuredEnvironment(), buildOptions, "provide exactly one input source",
        "config without CLI source should be rejected");

    options = {};
    options.pcapPath = "samples/arp.pcap";
    options.interfaceName = "eth0";
    expectBuildErrorContains(options, configuredEnvironment(), buildOptions, "provide exactly one input source",
        "conflicting CLI sources should be rejected after merge");

    options = {};
    options.pcapPath = "samples/arp.pcap";
    expectBuildErrorContains(options, RuntimeEnvironment{}, buildOptions, "PostgreSQL configuration is required",
        "missing database environment should be rejected");
}

} // namespace

int main()
{
    loadsSupportedYamlSubset();
    rejectsInvalidYamlAndSensitiveFields();
    mergesPrecedenceDeterministically();
    builtInOutputDefaultsToJson();
    loadsProfilesAndHandlesMissingFiles();
    validatesMergedConfig();

    if (failures > 0) {
        std::cerr << failures << " app config test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
