#pragma once

#include <string>

namespace asset_discovery::capture {

class PacketCaptureBackend {
public:
    bool pcapAvailable() const;
    std::string backendName() const;
};

} // namespace asset_discovery::capture
