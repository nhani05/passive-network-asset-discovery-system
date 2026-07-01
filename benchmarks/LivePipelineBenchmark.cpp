#include "application/live/LiveCapturePipeline.hpp"
#include "infrastructure/capture/PacketCapture.hpp"

#include <charconv>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

struct BenchmarkOptions {
    std::optional<std::string> pcapPath;
    std::optional<std::string> interfaceName;
    std::optional<int> durationSeconds;
    std::size_t workerCount = 0;
    std::size_t batchSize = 128;
    std::optional<std::string> packetFilter;
    asset_discovery::capture::CaptureBackendSelection backend =
        asset_discovery::capture::CaptureBackendSelection::Pcap;
};

std::optional<std::size_t> parsePositiveSize(const std::string& value)
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

std::optional<int> parsePositiveInt(const std::string& value)
{
    int parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end || parsed <= 0) {
        return std::nullopt;
    }
    return parsed;
}

void printUsage(const char* executable)
{
    std::cerr << "usage:\n"
              << "  " << executable << " <pcap-path> [worker-count] [batch-size] [filter]\n"
              << "  " << executable << " --pcap <path> [--workers <count>] [--batch-size <count>] [--filter <bpf>]\n"
              << "  " << executable << " --interface <name> --duration <seconds> --backend pcap|af-packet|auto [--workers <count>] [--batch-size <count>] [--filter <bpf>]\n";
}

std::optional<std::string> parseOptions(int argc, char* argv[], BenchmarkOptions& options)
{
    if (argc >= 2 && std::string(argv[1]).rfind("--", 0) != 0) {
        if (argc > 5) {
            return "too many positional arguments";
        }
        options.pcapPath = argv[1];
        if (argc >= 3) {
            const auto parsed = parsePositiveSize(argv[2]);
            if (!parsed.has_value()) {
                return "worker-count must be a positive integer";
            }
            options.workerCount = *parsed;
        }
        if (argc >= 4) {
            const auto parsed = parsePositiveSize(argv[3]);
            if (!parsed.has_value()) {
                return "batch-size must be a positive integer";
            }
            options.batchSize = *parsed;
        }
        if (argc == 5) {
            options.packetFilter = argv[4];
            if (options.packetFilter->empty()) {
                return "filter cannot be empty";
            }
        }
        return std::nullopt;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto needsValue = [&]() {
            return i + 1 >= argc;
        };

        if (arg == "--pcap") {
            if (needsValue()) {
                return "--pcap requires a path";
            }
            options.pcapPath = argv[++i];
            continue;
        }
        if (arg == "--interface") {
            if (needsValue()) {
                return "--interface requires a name";
            }
            options.interfaceName = argv[++i];
            continue;
        }
        if (arg == "--duration") {
            if (needsValue()) {
                return "--duration requires seconds";
            }
            const auto parsed = parsePositiveInt(argv[++i]);
            if (!parsed.has_value()) {
                return "--duration must be a positive integer";
            }
            options.durationSeconds = *parsed;
            continue;
        }
        if (arg == "--workers") {
            if (needsValue()) {
                return "--workers requires a count";
            }
            const auto parsed = parsePositiveSize(argv[++i]);
            if (!parsed.has_value()) {
                return "--workers must be a positive integer";
            }
            options.workerCount = *parsed;
            continue;
        }
        if (arg == "--batch-size") {
            if (needsValue()) {
                return "--batch-size requires a count";
            }
            const auto parsed = parsePositiveSize(argv[++i]);
            if (!parsed.has_value()) {
                return "--batch-size must be a positive integer";
            }
            options.batchSize = *parsed;
            continue;
        }
        if (arg == "--filter") {
            if (needsValue()) {
                return "--filter requires a BPF expression";
            }
            options.packetFilter = argv[++i];
            if (options.packetFilter->empty()) {
                return "filter cannot be empty";
            }
            continue;
        }
        if (arg == "--backend") {
            if (needsValue()) {
                return "--backend requires one of: pcap, af-packet, auto";
            }
            const auto parsed = asset_discovery::capture::parseCaptureBackendSelection(argv[++i]);
            if (!parsed.has_value()) {
                return "backend must be one of: pcap, af-packet, auto";
            }
            options.backend = *parsed;
            continue;
        }
        return "unknown argument '" + arg + "'";
    }

    if (options.pcapPath.has_value() == options.interfaceName.has_value()) {
        return "provide exactly one traffic source: --pcap <path> or --interface <name>";
    }
    if (options.interfaceName.has_value() && !options.durationSeconds.has_value()) {
        return "--interface benchmark requires --duration <seconds>";
    }
    return std::nullopt;
}

asset_discovery::live::LivePipelineOptions pipelineOptions(const BenchmarkOptions& options)
{
    asset_discovery::live::LivePipelineOptions pipeline;
    pipeline.packetBatchSize = options.batchSize;
    pipeline.packetQueueCapacity = 16384;
    pipeline.observationQueueCapacity = 16384;
    pipeline.parserWorkerCount = options.workerCount;
    return pipeline;
}

int runOfflineBenchmark(const BenchmarkOptions& options)
{
    const asset_discovery::capture::PacketCaptureBackend backend;
    const auto pcapResult = backend.readPcapFile(*options.pcapPath, options.packetFilter);
    if (pcapResult.error.has_value()) {
        std::cerr << "error: " << *pcapResult.error << "\n";
        return 1;
    }

    const auto result = asset_discovery::live::processPacketsConcurrently(
        pcapResult.packets,
        pipelineOptions(options));
    std::cout << "benchmark=live-pipeline-offline-stress\n"
              << "target_packet_rate_per_second=1000000\n"
              << "traffic_source=user-provided-pcap\n"
              << "pcap_path=" << *options.pcapPath << "\n"
              << "packet_count=" << pcapResult.packets.size() << "\n"
              << "filter=" << (options.packetFilter.has_value() ? *options.packetFilter : "<none>") << "\n"
              << "build_type=Release recommended\n"
              << asset_discovery::live::formatLivePipelineMetrics(result.stats);

    if (result.error.has_value()) {
        std::cerr << "error: " << *result.error << "\n";
        return 1;
    }
    return 0;
}

int runLiveBackendBenchmark(const BenchmarkOptions& options)
{
    auto backendResult = asset_discovery::capture::createCaptureBackend(options.backend);
    if (backendResult.error.has_value() || !backendResult.backend) {
        std::cerr << "error: " << backendResult.error.value_or("capture backend could not be created") << "\n";
        return 1;
    }

    asset_discovery::capture::CaptureConfig config;
    config.interfaceName = *options.interfaceName;
    config.packetFilter = options.packetFilter;
    config.requestedBackend = options.backend;
    config.liveOptions.durationSeconds = options.durationSeconds;

    const auto result = asset_discovery::live::runLiveCapturePipeline(
        *backendResult.backend,
        std::move(config),
        std::nullopt,
        std::move(backendResult.initialStats),
        pipelineOptions(options));

    std::cout << "benchmark=live-backend-comparison\n"
              << "traffic_source=live-interface\n"
              << "interface=" << *options.interfaceName << "\n"
              << "duration_seconds=" << *options.durationSeconds << "\n"
              << "filter=" << (options.packetFilter.has_value() ? *options.packetFilter : "<none>") << "\n"
              << "build_type=Release recommended\n"
              << asset_discovery::live::formatLivePipelineMetrics(result.stats);

    if (result.error.has_value()) {
        std::cerr << "error: " << *result.error << "\n";
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    BenchmarkOptions options;
    const auto parseError = parseOptions(argc, argv, options);
    if (parseError.has_value()) {
        std::cerr << "error: " << *parseError << "\n";
        printUsage(argv[0]);
        return 2;
    }

    if (options.pcapPath.has_value()) {
        return runOfflineBenchmark(options);
    }
    return runLiveBackendBenchmark(options);
}
