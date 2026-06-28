#include "asset/AssetStore.hpp"
#include "capture/PacketCapture.hpp"
#include "cli/Arguments.hpp"
#include "parser/ArpPacket.hpp"

#include <iostream>
#include <iterator>
#include <set>
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

std::string joinIpAddresses(const std::set<std::string>& ipAddresses)
{
    std::string output;
    for (const auto& ipAddress : ipAddresses) {
        if (!output.empty()) {
            output += ",";
        }
        output += ipAddress;
    }
    return output;
}

std::string joinSources(const std::set<asset_discovery::parser::ObservationSource>& sources)
{
    std::string output;
    for (const auto& source : sources) {
        if (!output.empty()) {
            output += ",";
        }
        output += asset_discovery::parser::observationSourceName(source);
    }
    return output;
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

        asset_discovery::asset::AssetStore assetStore;
        for (const auto& observation : observations) {
            assetStore.applyObservation(observation);
        }

        const auto assets = assetStore.assets();
        std::cout << "assets=" << assets.size() << "\n";
        for (const auto& asset : assets) {
            std::cout << "asset mac=" << asset.macAddress
                      << " ips=" << joinIpAddresses(asset.ipAddresses)
                      << " first_seen=" << asset_discovery::asset::formatTimestamp(asset.firstSeen)
                      << " last_seen=" << asset_discovery::asset::formatTimestamp(asset.lastSeen)
                      << " sources=" << joinSources(asset.sources) << "\n";
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
