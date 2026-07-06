#pragma once

#include "pnad/capture/PacketCapture.hpp"

#include <string>
#include <vector>

namespace asset_discovery::capture {

struct NetworkInterfaceAddress {
    std::string address;
    unsigned int prefixLength = 0;
    bool ipv6 = false;
};

struct NetworkInterfaceInfo {
    std::string systemName;
    std::string displayName;
    std::string macAddress;
    std::vector<NetworkInterfaceAddress> addresses;
    bool isUp = false;
    bool isRunning = false;
    bool isLoopback = false;
    bool isVirtual = false;
    bool pcapAvailable = false;
    std::string pcapDiagnostic;
    bool captureAllowed = false;
    std::string permissionDiagnostic;
    std::string refreshedAt;
};

int networkInterfaceDisplayPriority(const NetworkInterfaceInfo& interfaceInfo);
void sortNetworkInterfacesForDisplay(std::vector<NetworkInterfaceInfo>& interfaces);
std::vector<NetworkInterfaceInfo> listNetworkInterfaces();

} // namespace asset_discovery::capture
