#pragma once

#include "pnad/capture/PacketCapture.hpp"

#include <cstddef>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace asset_discovery::capture {

struct PacketStreamReadResult {
    std::vector<OfflinePacket> packets;
    std::optional<std::string> error;
    bool endOfStream = false;
};

bool writePacketStreamFrame(std::ostream& output, const OfflinePacket& packet);
PacketStreamReadResult readPacketStreamFrames(std::istream& input, std::size_t maxFrames);
std::string packetStreamFormatName();

} // namespace asset_discovery::capture
