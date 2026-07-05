#include "pnad/backend/BackendConfig.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>

namespace asset_discovery::backend {
namespace {

std::optional<std::string> requireValue(
    const std::vector<std::string>& arguments,
    std::size_t& index,
    const std::string& option)
{
    if (index + 1 >= arguments.size()) {
        return option + " requires a value";
    }
    ++index;
    return std::nullopt;
}

std::optional<std::uint16_t> parsePort(const std::string& value)
{
    try {
        std::size_t consumed = 0;
        const unsigned long parsed = std::stoul(value, &consumed, 10);
        if (consumed != value.size()
            || parsed == 0
            || parsed > std::numeric_limits<std::uint16_t>::max()) {
            return std::nullopt;
        }
        return static_cast<std::uint16_t>(parsed);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace

BackendParseResult parseBackendArguments(const std::vector<std::string>& arguments)
{
    BackendParseResult result;

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto& option = arguments[index];
        if (option == "--help" || option == "-h") {
            result.helpRequested = true;
            return result;
        }

        const auto require = [&]() {
            return requireValue(arguments, index, option);
        };

        if (option == "--listen-address") {
            if (auto error = require(); error.has_value()) {
                result.error = error;
                return result;
            }
            result.config.listenAddress = arguments[index];
        } else if (option == "--port") {
            if (auto error = require(); error.has_value()) {
                result.error = error;
                return result;
            }
            const auto port = parsePort(arguments[index]);
            if (!port.has_value()) {
                result.error = "--port must be a number from 1 to 65535";
                return result;
            }
            result.config.port = *port;
        } else if (option == "--sqlite") {
            if (auto error = require(); error.has_value()) {
                result.error = error;
                return result;
            }
            result.config.sqlitePath = arguments[index];
        } else if (option == "--database-url") {
            if (auto error = require(); error.has_value()) {
                result.error = error;
                return result;
            }
            result.config.databaseUrl = arguments[index];
        } else if (option == "--capture-mode") {
            if (auto error = require(); error.has_value()) {
                result.error = error;
                return result;
            }
            if (arguments[index] == "live") {
                result.config.captureMode = BackendCaptureMode::Live;
            } else if (arguments[index] == "pcap") {
                result.config.captureMode = BackendCaptureMode::PcapOffline;
            } else {
                result.error = "--capture-mode must be live or pcap";
                return result;
            }
        } else if (option == "--interface") {
            if (auto error = require(); error.has_value()) {
                result.error = error;
                return result;
            }
            result.config.interfaceName = arguments[index];
        } else if (option == "--pcap") {
            if (auto error = require(); error.has_value()) {
                result.error = error;
                return result;
            }
            result.config.captureMode = BackendCaptureMode::PcapOffline;
            result.config.pcapPath = arguments[index];
        } else if (option == "--filter") {
            if (auto error = require(); error.has_value()) {
                result.error = error;
                return result;
            }
            result.config.packetFilter = arguments[index];
        } else if (option == "--runtime-log") {
            if (auto error = require(); error.has_value()) {
                result.error = error;
                return result;
            }
            result.config.runtimeLogPath = arguments[index];
        } else if (option == "--serve") {
            result.config.serve = true;
        } else {
            result.error = "unknown option: " + option;
            return result;
        }
    }

    return result;
}

std::string backendUsage()
{
    std::ostringstream output;
    output
        << "assetd - Passive Network Asset Discovery backend service\n"
        << "\n"
        << "Usage:\n"
        << "  assetd [options]\n"
        << "\n"
        << "Options:\n"
        << "  --listen-address <address>  Address to bind REST/WebSocket services (default: 127.0.0.1)\n"
        << "  --port <port>               Port to bind REST/WebSocket services (default: 8080)\n"
        << "  --sqlite <path>             SQLite database path (default: pnad.db)\n"
        << "  --database-url <url>        PostgreSQL connection URL\n"
        << "  --capture-mode <live|pcap>  Capture mode for service requests (default: live)\n"
        << "  --interface <name>          Live capture interface\n"
        << "  --pcap <path>               PCAP file for offline analysis\n"
        << "  --filter <bpf>              Capture filter\n"
        << "  --runtime-log <path>        Backend runtime log path\n"
        << "  --serve                     Run the blocking HTTP service loop\n"
        << "  --help                      Show this help\n";
    return output.str();
}

std::string captureModeName(BackendCaptureMode mode)
{
    switch (mode) {
    case BackendCaptureMode::Live:
        return "live";
    case BackendCaptureMode::PcapOffline:
        return "pcap";
    }
    return "live";
}

} // namespace asset_discovery::backend
