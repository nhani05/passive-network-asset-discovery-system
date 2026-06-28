#include "capture/PacketCapture.hpp"
#include "cli/Arguments.hpp"
#include "parser/ArpPacket.hpp"

#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

asset_discovery::parser::ObservationTimestamp toObservationTimestamp(
    const asset_discovery::capture::PacketTimestamp& timestamp)
{
    return {timestamp.seconds, timestamp.microseconds};
}

std::vector<asset_discovery::parser::AssetObservation> collectArpObservations(
    const std::vector<asset_discovery::capture::OfflinePacket>& packets)
{
    std::vector<asset_discovery::parser::AssetObservation> observations;
    for (const auto& packet : packets) {
        if (packet.linkType != asset_discovery::capture::LinkType::Ethernet) {
            continue;
        }

        auto packetObservations = asset_discovery::parser::parseArpObservationsFromEthernetFrame(
            packet.bytes,
            toObservationTimestamp(packet.timestamp));
        observations.insert(
            observations.end(),
            std::make_move_iterator(packetObservations.begin()),
            std::make_move_iterator(packetObservations.end()));
    }
    return observations;
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
        const auto pcapResult = backend.readPcapFile(*result.options.pcapPath);
        if (pcapResult.error.has_value()) {
            std::cerr << "error: " << *pcapResult.error << "\n";
            return 1;
        }

        std::cout << "mode=pcap path=" << *result.options.pcapPath << "\n"
                  << "packets=" << pcapResult.packets.size() << "\n";
        if (!pcapResult.packets.empty()) {
            std::cout << "link_type="
                      << asset_discovery::capture::linkTypeName(pcapResult.packets.front().linkType) << "\n";
        }

        const auto observations = collectArpObservations(pcapResult.packets);
        std::cout << "observations=" << observations.size() << "\n";
        for (const auto& observation : observations) {
            std::cout << "observation source="
                      << asset_discovery::parser::observationSourceName(observation.source)
                      << " mac=" << observation.macAddress;
            if (observation.ipAddress.has_value()) {
                std::cout << " ip=" << *observation.ipAddress;
            }
            std::cout << " timestamp=" << observation.timestamp.seconds << "."
                      << observation.timestamp.microseconds << "\n";
        }
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
