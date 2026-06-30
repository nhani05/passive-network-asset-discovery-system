#include "application/live/LiveCapturePipeline.hpp"
#include "infrastructure/output/JsonRenderer.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using asset_discovery::capture::LinkType;
using asset_discovery::capture::OfflinePacket;
using asset_discovery::live::LivePipelineOptions;
using asset_discovery::live::formatLivePipelineMetrics;
using asset_discovery::live::processPacketsConcurrently;
using asset_discovery::output::renderAssetJson;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

std::vector<std::uint8_t> macBytes(const std::string& value)
{
    std::vector<std::uint8_t> result;
    std::istringstream input(value);
    std::string part;
    while (std::getline(input, part, ':')) {
        result.push_back(static_cast<std::uint8_t>(std::stoul(part, nullptr, 16)));
    }
    return result;
}

std::vector<std::uint8_t> ipv4Bytes(const std::string& value)
{
    std::vector<std::uint8_t> result;
    std::istringstream input(value);
    std::string part;
    while (std::getline(input, part, '.')) {
        result.push_back(static_cast<std::uint8_t>(std::stoul(part)));
    }
    return result;
}

void appendUInt16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void appendBytes(std::vector<std::uint8_t>& bytes, const std::vector<std::uint8_t>& suffix)
{
    bytes.insert(bytes.end(), suffix.begin(), suffix.end());
}

OfflinePacket arpPacket(
    const std::string& sourceMac,
    const std::string& sourceIp,
    std::int64_t seconds)
{
    std::vector<std::uint8_t> bytes;
    appendBytes(bytes, macBytes("ff:ff:ff:ff:ff:ff"));
    appendBytes(bytes, macBytes(sourceMac));
    appendUInt16(bytes, 0x0806);
    appendUInt16(bytes, 1);
    appendUInt16(bytes, 0x0800);
    bytes.push_back(6);
    bytes.push_back(4);
    appendUInt16(bytes, 1);
    appendBytes(bytes, macBytes(sourceMac));
    appendBytes(bytes, ipv4Bytes(sourceIp));
    appendBytes(bytes, macBytes("00:00:00:00:00:00"));
    appendBytes(bytes, ipv4Bytes("192.168.1.1"));

    OfflinePacket packet;
    packet.timestamp.seconds = seconds;
    packet.linkType = LinkType::Ethernet;
    packet.capturedLength = static_cast<std::uint32_t>(bytes.size());
    packet.originalLength = static_cast<std::uint32_t>(bytes.size());
    packet.bytes = std::move(bytes);
    return packet;
}

void parsesPacketsAndAggregatesAssets()
{
    const std::vector<OfflinePacket> packets = {
        arpPacket("02:42:ac:11:00:02", "192.168.1.10", 10),
        arpPacket("02:42:ac:11:00:03", "192.168.1.11", 11),
    };
    LivePipelineOptions options;
    options.packetBatchSize = 1;
    options.packetQueueCapacity = 4;
    options.observationQueueCapacity = 4;
    options.parserWorkerCount = 2;

    const auto result = processPacketsConcurrently(packets, options);

    expect(!result.error.has_value(), "pipeline should process valid packets without error");
    expect(result.assets.size() == 2, "aggregator should produce one asset per MAC");
    expect(result.stats.packetsCaptured == 2, "stats should count captured packets");
    expect(result.stats.packetsEnqueued == 2, "stats should count enqueued packets");
    expect(result.stats.packetsParsed == 2, "stats should count parsed packets");
    expect(result.stats.observationsProduced == 2, "stats should count produced observations");
    expect(result.stats.observationsApplied == 2, "stats should count applied observations");
    expect(result.stats.parserWorkerCount == 2, "stats should preserve worker count");
}

void keepsMetricsSeparateFromJsonAssets()
{
    const std::vector<OfflinePacket> packets = {
        arpPacket("02:42:ac:11:00:04", "192.168.1.12", 12),
    };
    LivePipelineOptions options;
    options.packetBatchSize = 1;
    options.packetQueueCapacity = 2;
    options.observationQueueCapacity = 2;
    options.parserWorkerCount = 1;

    const auto result = processPacketsConcurrently(packets, options);
    const auto json = renderAssetJson(result.assets);
    const auto metrics = formatLivePipelineMetrics(result.stats);

    expect(json.find("live_capture_metrics") == std::string::npos, "asset JSON should not include metrics");
    expect(metrics.find("live_capture_metrics") != std::string::npos, "metrics should use separate text format");
    expect(metrics.find("packets_parsed=1") != std::string::npos, "metrics should report parsed packets");
}

} // namespace

int main()
{
    parsesPacketsAndAggregatesAssets();
    keepsMetricsSeparateFromJsonAssets();

    if (failures != 0) {
        std::cerr << failures << " live capture pipeline test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
