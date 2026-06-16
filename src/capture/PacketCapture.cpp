#include "capture/PacketCapture.hpp"

namespace asset_discovery::capture {

bool PacketCaptureBackend::pcapAvailable() const
{
    return ASSET_DISCOVERY_HAS_PCAP == 1;
}

std::string PacketCaptureBackend::backendName() const
{
    return ASSET_DISCOVERY_HAS_PCAP == 1 ? "libpcap" : "libpcap-unavailable";
}

} // namespace asset_discovery::capture
