#include "asset/AssetStore.hpp"
#include "capture/PacketCapture.hpp"
#include "cli/Arguments.hpp"
#include "output/TableRenderer.hpp"
#include "parser/PacketParsers.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

asset_discovery::parser::ObservationTimestamp toObservationTimestamp(
    const asset_discovery::capture::PacketTimestamp& timestamp)
{
    return {timestamp.seconds, timestamp.microseconds};
}

asset_discovery::asset::AssetStore buildAssetStore(
    const std::vector<asset_discovery::capture::OfflinePacket>& packets)
{
    asset_discovery::asset::AssetStore store;
    for (const auto& packet : packets) {
        if (packet.linkType != asset_discovery::capture::LinkType::Ethernet) {
            continue;
        }

        const auto observations = asset_discovery::parser::parseEthernetObservations(
            packet.bytes,
            toObservationTimestamp(packet.timestamp));
        for (const auto& observation : observations) {
            store.applyObservation(observation);
        }
    }
    return store;
}

} // namespace

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
        std::cerr << "lỗi: " << *result.error << "\n\n"
                  << asset_discovery::cli::usageText(executableName);
        return 2;
    }

    const asset_discovery::capture::PacketCaptureBackend backend;
    if (!backend.pcapAvailable()) {
        std::cerr << "cảnh báo: backend " << backend.backendName()
                  << " không có trong bản build này; chức năng capture packet sẽ hoạt động khi cài libpcap.\n";
    }

    if (result.options.pcapPath.has_value()) {
        const auto pcapResult = backend.readPcapFile(*result.options.pcapPath);
        if (pcapResult.error.has_value()) {
            std::cerr << "lỗi: " << *pcapResult.error << "\n";
            return 1;
        }

        const auto assetStore = buildAssetStore(pcapResult.packets);
        if (result.options.outputFormat == asset_discovery::cli::OutputFormat::Table) {
            std::cout << asset_discovery::output::renderAssetTable(assetStore.assets());
        } else {
            std::cerr << "error: JSON output is not implemented yet\n";
            return 1;
        }
    } else if (result.options.interfaceName.has_value()) {
        std::cout << "mode=interface name=" << *result.options.interfaceName;
        if (result.options.durationSeconds.has_value()) {
            std::cout << " duration=" << *result.options.durationSeconds;
        }
        std::cout << "\n";
    }

    return 0;
}
