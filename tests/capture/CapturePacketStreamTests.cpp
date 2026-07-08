#include "pnad/capture/CapturePacketStream.hpp"

#include <cassert>
#include <sstream>

int main()
{
    asset_discovery::capture::OfflinePacket packet;
    packet.timestamp.seconds = 1700000000;
    packet.timestamp.microseconds = 42;
    packet.linkType = asset_discovery::capture::LinkType::Ethernet;
    packet.capturedLength = 4;
    packet.originalLength = 6;
    packet.bytes = {0xde, 0xad, 0xbe, 0xef};

    std::stringstream stream;
    assert(asset_discovery::capture::writePacketStreamFrame(stream, packet));
    assert(asset_discovery::capture::packetStreamFormatName() == "PNAD_PACKET_V1");

    const auto result = asset_discovery::capture::readPacketStreamFrames(stream, 16);
    assert(!result.error.has_value());
    assert(result.packets.size() == 1);
    assert(result.packets.front().timestamp.seconds == packet.timestamp.seconds);
    assert(result.packets.front().timestamp.microseconds == packet.timestamp.microseconds);
    assert(result.packets.front().capturedLength == packet.capturedLength);
    assert(result.packets.front().originalLength == packet.originalLength);
    assert(result.packets.front().bytes == packet.bytes);

    std::stringstream bad("not-a-frame\n");
    const auto badResult = asset_discovery::capture::readPacketStreamFrames(bad, 1);
    assert(badResult.error.has_value());

    return 0;
}
