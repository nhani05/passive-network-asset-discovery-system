#include "capture/PacketCapture.hpp"
#include "cli/Arguments.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const std::string executableName = argc > 0 ? argv[0] : "asset-discovery";
    const auto result = asset_discovery::cli::parseArguments(args);

    if (result.options.helpRequested) {
        std::cout << asset_discovery::cli::usageText(executableName);
        return 0;
    }

    if (result.error.has_value()) {
        std::cerr << "error: " << *result.error << "\n\n"
                  << asset_discovery::cli::usageText(executableName);
        return 2;
    }

    const asset_discovery::capture::PacketCaptureBackend backend;
    if (!backend.pcapAvailable()) {
        std::cerr << "warning: " << backend.backendName()
                  << " backend is not available in this build; capture implementation will be enabled when libpcap is installed.\n";
    }

    if (result.options.pcapPath.has_value()) {
        std::cout << "mode=pcap path=" << *result.options.pcapPath << "\n";
    } else if (result.options.interfaceName.has_value()) {
        std::cout << "mode=interface name=" << *result.options.interfaceName;
        if (result.options.durationSeconds.has_value()) {
            std::cout << " duration=" << *result.options.durationSeconds;
        }
        std::cout << "\n";
    }

    std::cout << "output=" << asset_discovery::cli::outputFormatName(result.options.outputFormat) << "\n";
    return 0;
}
