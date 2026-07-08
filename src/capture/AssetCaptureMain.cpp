#include "pnad/capture/PacketCapture.hpp"
#include "pnad/capture/NetworkInterface.hpp"
#include "pnad/capture/CaptureChildProtocol.hpp"
#include "pnad/cli/Arguments.hpp"
#include "pnad/constants/CliConstants.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool hasArg(const std::vector<std::string>& args, const std::string& value)
{
    return std::find(args.begin(), args.end(), value) != args.end();
}

std::string usageText(const std::string& executableName)
{
    return "Usage: " + executableName + " [--help] [--version] [--backend-status]\n"
        "\n"
        "asset-capture is the live capture child process used by the desktop GUI/core\n"
        "process. It owns raw packet capture backend access; parent supervision,\n"
        "control messages, and capture-output tailing are implemented in later slices.\n"
        "\n"
        "Options:\n"
        "  --help            Show this help text\n"
        "  --version         Show product version\n"
        "  --backend-status  Print capture backend availability and exit\n"
        "  --self-test-protocol\n"
        "                    Emit sample child protocol messages and exit\n";
}

int printBackendStatus()
{
    const auto permission = asset_discovery::capture::probePacketCapturePermission();
    const auto interfaces = asset_discovery::capture::listNetworkInterfaces();
    const auto captureReadyCount = std::count_if(
        interfaces.begin(),
        interfaces.end(),
        [](const auto& interfaceInfo) {
            return interfaceInfo.captureAllowed;
        });

    auto result = asset_discovery::capture::createCaptureBackend(
        asset_discovery::capture::CaptureBackendSelection::Auto);
    if (result.error.has_value() || !result.backend) {
        std::cerr << "capture_backend_status available=false reason=\""
                  << result.error.value_or("capture backend could not be created")
                  << "\" raw_socket_permission="
                  << asset_discovery::capture::packetCapturePermissionStateName(permission.state)
                  << " visible_interfaces=" << interfaces.size()
                  << " capture_allowed_interfaces=" << captureReadyCount;
        if (!permission.diagnostic.empty()) {
            std::cerr << " raw_socket_diagnostic=\"" << permission.diagnostic << "\"";
        }
        std::cerr << "\n";
        return 3;
    }

    const auto availability = result.backend->availability();
    std::cout << "capture_backend_status"
              << " backend=" << result.backend->backendName()
              << " available=" << (availability.available ? "true" : "false")
              << " raw_socket_permission="
              << asset_discovery::capture::packetCapturePermissionStateName(permission.state)
              << " visible_interfaces=" << interfaces.size()
              << " capture_allowed_interfaces=" << captureReadyCount;
    if (!availability.reason.empty()) {
        std::cout << " reason=\"" << availability.reason << "\"";
    }
    if (!permission.diagnostic.empty()) {
        std::cout << " raw_socket_diagnostic=\"" << permission.diagnostic << "\"";
    }
    std::cout << "\n";
    return availability.available ? 0 : 3;
}

int emitProtocolSelfTest()
{
    using asset_discovery::capture::CaptureChildMessageType;
    using asset_discovery::capture::serializeCaptureChildMessage;

    std::cout << serializeCaptureChildMessage({
        CaptureChildMessageType::Started,
        0,
        0,
        "/tmp/pnad-child-self-test.pcap",
        "capture child started",
        {{"backend", "self-test"}}
    });
    std::cout << serializeCaptureChildMessage({
        CaptureChildMessageType::NewData,
        1,
        0,
        "/tmp/pnad-child-self-test.pcap",
        "new packet records available",
        {}
    });
    std::cout << serializeCaptureChildMessage({
        CaptureChildMessageType::Stopped,
        1,
        0,
        "/tmp/pnad-child-self-test.pcap",
        "capture child stopped",
        {}
    });
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    const std::string executableName = argc > 0
        ? argv[0]
        : asset_discovery::constants::cli::CaptureExecutableName;
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.empty()
        || hasArg(args, asset_discovery::constants::cli::HelpOption)
        || hasArg(args, asset_discovery::constants::cli::ShortHelpOption)) {
        std::cout << usageText(executableName);
        return 0;
    }

    if (hasArg(args, asset_discovery::constants::cli::VersionOption)) {
        std::cout << asset_discovery::cli::versionText();
        return 0;
    }

    if (hasArg(args, "--backend-status")) {
        return printBackendStatus();
    }

    if (hasArg(args, "--self-test-protocol")) {
        return emitProtocolSelfTest();
    }

    std::cerr << "asset-capture protocol mode is not implemented yet\n\n"
              << usageText(executableName);
    return 2;
}
