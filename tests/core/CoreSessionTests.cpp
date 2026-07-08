#include "pnad/core/CoreSession.hpp"
#include "pnad/capture/CapturePacketStream.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> arpRequestFrame()
{
    return {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x02, 0x42, 0xac, 0x11, 0x00, 0x02,
        0x08, 0x06,
        0x00, 0x01,
        0x08, 0x00,
        0x06,
        0x04,
        0x00, 0x01,
        0x02, 0x42, 0xac, 0x11, 0x00, 0x02,
        0xc0, 0xa8, 0x01, 0x0a,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xc0, 0xa8, 0x01, 0x01
    };
}

asset_discovery::capture::OfflinePacket makePacket()
{
    asset_discovery::capture::OfflinePacket packet;
    packet.timestamp.seconds = 1700000000;
    packet.timestamp.microseconds = 123456;
    packet.linkType = asset_discovery::capture::LinkType::Ethernet;
    packet.bytes = arpRequestFrame();
    packet.capturedLength = static_cast<std::uint32_t>(packet.bytes.size());
    packet.originalLength = packet.capturedLength;
    return packet;
}

} // namespace

int main()
{
    using namespace asset_discovery::core;

    std::vector<SessionEvent> events;
    std::vector<asset_discovery::asset::Asset> assetUpdates;
    std::vector<asset_discovery::asset::Asset> snapshot;
    SessionProgress lastProgress;

    CoreSession session({
        [&](const SessionEvent& event) {
            events.push_back(event);
        },
        [&](const asset_discovery::asset::Asset& asset) {
            assetUpdates.push_back(asset);
        },
        [&](const std::vector<asset_discovery::asset::Asset>& assets) {
            snapshot = assets;
        },
        [&](const SessionProgress& progress) {
            lastProgress = progress;
        }
    });

    const auto result = session.processPacketBatch({makePacket()});

    assert(!result.error.has_value());
    assert(result.assets.size() == 1);
    assert(result.progress.packetsRead == 1);
    assert(result.progress.packetsParsed == 1);
    assert(lastProgress.packetsParsed == 1);
    assert(snapshot.size() == 1);
    assert(!assetUpdates.empty());
    assert(assetUpdates.front().macAddress == "02:42:ac:11:00:02");
    assert(assetUpdates.front().ipAddresses.count("192.168.1.10") == 1);

    const auto hasCaptureNewData = std::any_of(events.begin(), events.end(), [](const SessionEvent& event) {
        return event.type == SessionEventType::CaptureNewData;
    });
    const auto hasAssetCreated = std::any_of(events.begin(), events.end(), [](const SessionEvent& event) {
        return event.type == SessionEventType::AssetCreated;
    });
    const auto hasProgress = std::any_of(events.begin(), events.end(), [](const SessionEvent& event) {
        return event.type == SessionEventType::ProgressUpdated;
    });

    assert(hasCaptureNewData);
    assert(hasAssetCreated);
    assert(hasProgress);
    assert(sessionEventTypeName(SessionEventType::CaptureNewData) == "capture.new_data");
    assert(sessionEventSeverityName(SessionEventSeverity::Warning) == "warning");

    std::stringstream stream;
    assert(asset_discovery::capture::writePacketStreamFrame(stream, makePacket()));
    events.clear();
    assetUpdates.clear();
    snapshot.clear();
    lastProgress = {};

    const auto streamResult = session.processPacketStreamBatch(stream, 16);
    assert(!streamResult.error.has_value());
    assert(streamResult.assets.size() == 1);
    assert(lastProgress.packetsParsed == 1);
    assert(snapshot.size() == 1);
    assert(!assetUpdates.empty());
    const auto hasStreamNewData = std::any_of(events.begin(), events.end(), [](const SessionEvent& event) {
        return event.type == SessionEventType::CaptureNewData;
    });
    assert(hasStreamNewData);

    return 0;
}
