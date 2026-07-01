#pragma once

#include "infrastructure/capture/PacketCapture.hpp"

namespace asset_discovery::capture {

class AfPacketCaptureBackend final : public CaptureBackend {
public:
    std::string backendName() const override;
    BackendAvailability availability() const override;
    bool supportsOfflinePcap() const override;

    PcapReadResult readPcapFile(
        const std::string& path,
        std::optional<std::string> packetFilter = std::nullopt) const override;

    std::optional<std::string> captureLive(
        const CaptureConfig& config,
        LiveCaptureCallback callback,
        BackendStats* stats = nullptr) const override;
};

} // namespace asset_discovery::capture
