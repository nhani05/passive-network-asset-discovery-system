#include "application/live/LiveCapturePipeline.hpp"
#include "infrastructure/capture/PacketCapture.hpp"

#include <charconv>
#include <iostream>
#include <optional>
#include <string>

namespace {

std::optional<std::size_t> parsePositiveInteger(const std::string& value)
{
    std::size_t parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end || parsed == 0) {
        return std::nullopt;
    }
    return parsed;
}

} // namespace

int main(int argc, char* argv[])
{
    std::string pcapPath;
    std::size_t workerCount = 0;
    std::size_t batchSize = 128;
    std::optional<std::string> packetFilter;

    if (argc < 2 || argc > 5) {
        std::cerr << "usage: " << argv[0] << " <pcap-path> [worker-count] [batch-size] [filter]\n";
        return 2;
    }
    pcapPath = argv[1];
    if (argc >= 3) {
        const auto parsed = parsePositiveInteger(argv[2]);
        if (!parsed.has_value()) {
            std::cerr << "worker-count must be a positive integer\n";
            return 2;
        }
        workerCount = *parsed;
    }
    if (argc >= 4) {
        const auto parsed = parsePositiveInteger(argv[3]);
        if (!parsed.has_value()) {
            std::cerr << "batch-size must be a positive integer\n";
            return 2;
        }
        batchSize = *parsed;
    }
    if (argc == 5) {
        packetFilter = argv[4];
        if (packetFilter->empty()) {
            std::cerr << "filter cannot be empty\n";
            return 2;
        }
    }

    const asset_discovery::capture::PacketCaptureBackend backend;
    const auto pcapResult = backend.readPcapFile(pcapPath, packetFilter);
    if (pcapResult.error.has_value()) {
        std::cerr << "error: " << *pcapResult.error << "\n";
        return 1;
    }

    asset_discovery::live::LivePipelineOptions options;
    options.packetBatchSize = batchSize;
    options.packetQueueCapacity = 16384;
    options.observationQueueCapacity = 16384;
    options.parserWorkerCount = workerCount;

    const auto result = asset_discovery::live::processPacketsConcurrently(pcapResult.packets, options);
    std::cout << "benchmark=live-pipeline-offline-stress\n"
              << "target_packet_rate_per_second=1000000\n"
              << "traffic_source=user-provided-pcap\n"
              << "pcap_path=" << pcapPath << "\n"
              << "packet_count=" << pcapResult.packets.size() << "\n"
              << "filter=" << (packetFilter.has_value() ? *packetFilter : "<none>") << "\n"
              << "build_type=Release recommended\n"
              << asset_discovery::live::formatLivePipelineMetrics(result.stats);

    if (result.error.has_value()) {
        std::cerr << "error: " << *result.error << "\n";
        return 1;
    }
    return 0;
}
