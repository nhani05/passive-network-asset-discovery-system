#include "pnad/config/AppConfig.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

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

RuntimeEnvironment sqliteEnvironment()
{
    RuntimeEnvironment environment;
    environment.sqlitePath = "assets.db";
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
        "output:\n"
        "  format: csv\n");

    const auto result = loadConfigFile(path.string());
    expect(!result.error.has_value(), "valid YAML subset should load");
    expect(result.patch.outputFormat == OutputFormat::Csv,
        "output format should be parsed");
}

void rejectsRemovedYamlSections()
{
    const auto root = testRoot();

    const auto source = root / "source.yaml";
    writeFile(source, "capture:\n  interface: eth0\n");
    expectLoadErrorContains(source, "section 'capture' is no longer supported",
        "capture section should be rejected");

    const auto database = root / "database.yaml";
    writeFile(database, "database:\n  url: old-db-url\n");
    expectLoadErrorContains(database, "section 'database' is no longer supported",
        "database section should be rejected");

    const auto events = root / "events.yaml";
    writeFile(events, "events:\n  queue_capacity: 1024\n");
    expectLoadErrorContains(events, "section 'events' is no longer supported",
        "events section should be rejected");

    const auto network = root / "network.yaml";
    writeFile(network, "network:\n  ignore_nets: []\n");
    expectLoadErrorContains(network, "section 'network' is no longer supported",
        "network section should be rejected");
}

void rejectsInvalidYaml()
{
    const auto root = testRoot();

    const auto unknown = root / "unknown.yaml";
    writeFile(unknown, "capture:\n  queue_capacity: 10\n");
    expectLoadErrorContains(unknown, "section 'capture' is no longer supported", "capture section should be rejected");

    const auto tab = root / "tab.yaml";
    writeFile(tab, "output:\n\tformat: json\n");
    expectLoadErrorContains(tab, "tab indentation",
        "tab indentation should be rejected");
}

void mergesPrecedenceDeterministically()
{
    const auto root = testRoot();
    writeFile(root / "configs" / "default.yaml",
        "output:\n"
        "  format: json\n");

    asset_discovery::cli::Options options;
    options.pcapPath = "samples/arp.pcap";
    options.packetFilter = "arp";
    options.outputFormat = OutputFormat::Csv;
    options.outputFormatProvided = true;

    BuildConfigOptions buildOptions;
    buildOptions.defaultConfigPath = (root / "configs" / "default.yaml").string();

    const auto result = buildAppConfig(options, sqliteEnvironment(), buildOptions);
    expect(!result.error.has_value(), "merged config should validate");
    expect(result.config.capture.packetFilter == "arp",
        "CLI filter should override built-in config");
    expect(result.config.output.format == OutputFormat::Csv,
        "CLI output should override explicit config and default config");
    expect(result.config.database.sqlitePath == "assets.db",
        "SQLite environment path should be applied");
}

void builtInOutputDefaultsToJson()
{
    asset_discovery::cli::Options options;
    options.pcapPath = "samples/arp.pcap";

    BuildConfigOptions buildOptions;
    buildOptions.loadDefaultConfig = false;

    const auto result = buildAppConfig(options, sqliteEnvironment(), buildOptions);
    expect(!result.error.has_value(), "built-in defaults should validate with SQLite environment");
    expect(result.config.output.format == OutputFormat::Json,
        "built-in output format should default to json");
}

void validatesMergedConfig()
{
    const auto root = testRoot();
    BuildConfigOptions buildOptions;
    buildOptions.loadDefaultConfig = false;

    asset_discovery::cli::Options options;
    options.pcapPath = "samples/arp.pcap";

    options = {};
    expectBuildErrorContains(options, sqliteEnvironment(), buildOptions, "provide input source",
        "missing CLI source should be rejected");

    options = {};
    options.pcapPath = "samples/arp.pcap";
    expectBuildErrorContains(options, RuntimeEnvironment{}, buildOptions, "SQLite configuration is required",
        "missing SQLite configuration should be rejected");
}

} // namespace

int main()
{
    loadsSupportedYamlSubset();
    rejectsRemovedYamlSections();
    rejectsInvalidYaml();
    mergesPrecedenceDeterministically();
    builtInOutputDefaultsToJson();
    validatesMergedConfig();

    if (failures > 0) {
        std::cerr << failures << " app config test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
