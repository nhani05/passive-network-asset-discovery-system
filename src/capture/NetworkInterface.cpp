#include "pnad/capture/NetworkInterface.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>

#if defined(__linux__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace asset_discovery::capture {
namespace {

std::string refreshTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm utc = {};
#if defined(_WIN32)
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif

    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

bool hasPrefix(const std::string& value, const std::string& prefix)
{
    return value.rfind(prefix, 0) == 0;
}

bool isVirtualInterfaceName(const std::string& name)
{
    return hasPrefix(name, "br-")
        || hasPrefix(name, "docker")
        || hasPrefix(name, "veth")
        || hasPrefix(name, "virbr")
        || hasPrefix(name, "vmnet")
        || hasPrefix(name, "tun")
        || hasPrefix(name, "tap");
}

unsigned int countBits(const unsigned char* bytes, std::size_t length)
{
    unsigned int count = 0;
    for (std::size_t i = 0; i < length; ++i) {
        unsigned char value = bytes[i];
        for (int bit = 7; bit >= 0; --bit) {
            if ((value & (1U << bit)) == 0) {
                return count;
            }
            ++count;
        }
    }
    return count;
}

#if defined(__linux__)
unsigned int ipv4PrefixLength(const sockaddr* netmask)
{
    if (netmask == nullptr) {
        return 0;
    }
    const auto* address = reinterpret_cast<const sockaddr_in*>(netmask);
    const std::uint32_t mask = ntohl(address->sin_addr.s_addr);
    unsigned char bytes[] = {
        static_cast<unsigned char>((mask >> 24) & 0xff),
        static_cast<unsigned char>((mask >> 16) & 0xff),
        static_cast<unsigned char>((mask >> 8) & 0xff),
        static_cast<unsigned char>(mask & 0xff),
    };
    return countBits(bytes, sizeof(bytes));
}

unsigned int ipv6PrefixLength(const sockaddr* netmask)
{
    if (netmask == nullptr) {
        return 0;
    }
    const auto* address = reinterpret_cast<const sockaddr_in6*>(netmask);
    return countBits(address->sin6_addr.s6_addr, sizeof(address->sin6_addr.s6_addr));
}

std::string formatMacAddress(const sockaddr_ll* linkAddress)
{
    if (linkAddress == nullptr || linkAddress->sll_halen == 0) {
        return {};
    }

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (int i = 0; i < linkAddress->sll_halen; ++i) {
        if (i != 0) {
            output << ':';
        }
        output << std::setw(2) << static_cast<unsigned int>(linkAddress->sll_addr[i]);
    }
    return output.str();
}

std::string numericAddress(const sockaddr* address)
{
    char buffer[INET6_ADDRSTRLEN] = {};
    if (address->sa_family == AF_INET) {
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(address);
        if (inet_ntop(AF_INET, &ipv4->sin_addr, buffer, sizeof(buffer)) == nullptr) {
            return {};
        }
    } else if (address->sa_family == AF_INET6) {
        const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(address);
        if (inet_ntop(AF_INET6, &ipv6->sin6_addr, buffer, sizeof(buffer)) == nullptr) {
            return {};
        }
    }
    return buffer;
}
#endif

struct CapturePermissionProbe {
    bool allowed = false;
    std::string diagnostic;
};

CapturePermissionProbe probePacketCapturePermission()
{
#if defined(__linux__)
    const int socketFd = ::socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (socketFd >= 0) {
        ::close(socketFd);
        return {true, {}};
    }

    const int error = errno;
    if (error == EPERM || error == EACCES) {
        return {
            false,
            "Live Capture requires packet capture permission before use. "
            "Grant CAP_NET_RAW/CAP_NET_ADMIN to the PNAD application binary."
        };
    }

    return {
        false,
        std::string("could not verify packet capture permission: ") + std::strerror(error)
    };
#else
    return {true, {}};
#endif
}

void applyBackendDiagnostics(NetworkInterfaceInfo& interfaceInfo)
{
    const PcapCaptureBackend pcapBackend;
    const auto pcapAvailability = pcapBackend.availability();
    const auto permission = probePacketCapturePermission();
    interfaceInfo.pcapAvailable = pcapAvailability.available;
    interfaceInfo.pcapDiagnostic = pcapAvailability.reason;

    interfaceInfo.captureAllowed = interfaceInfo.isUp
        && !interfaceInfo.isLoopback
        && interfaceInfo.pcapAvailable
        && permission.allowed;

    if (!interfaceInfo.isUp) {
        interfaceInfo.permissionDiagnostic = "interface is down";
    } else if (interfaceInfo.isLoopback) {
        interfaceInfo.permissionDiagnostic = "loopback interface is not recommended for network discovery";
    } else if (!interfaceInfo.pcapAvailable) {
        if (!interfaceInfo.pcapDiagnostic.empty()) {
            interfaceInfo.permissionDiagnostic = interfaceInfo.pcapDiagnostic;
        } else {
            interfaceInfo.permissionDiagnostic = "no live capture backend is available";
        }
    } else if (!permission.allowed) {
        interfaceInfo.permissionDiagnostic = permission.diagnostic;
    }
}

} // namespace

int networkInterfaceDisplayPriority(const NetworkInterfaceInfo& interfaceInfo)
{
    int priority = 0;
    if (!interfaceInfo.isUp || !interfaceInfo.isRunning) {
        priority += 100;
    }
    if (interfaceInfo.isLoopback) {
        priority += 50;
    }
    if (interfaceInfo.isVirtual) {
        priority += 25;
    }
    if (interfaceInfo.addresses.empty()) {
        priority += 10;
    }
    if (!interfaceInfo.captureAllowed) {
        priority += 5;
    }
    return priority;
}

void sortNetworkInterfacesForDisplay(std::vector<NetworkInterfaceInfo>& interfaces)
{
    std::sort(interfaces.begin(), interfaces.end(), [](const auto& left, const auto& right) {
        const int leftPriority = networkInterfaceDisplayPriority(left);
        const int rightPriority = networkInterfaceDisplayPriority(right);
        if (leftPriority != rightPriority) {
            return leftPriority < rightPriority;
        }
        return left.systemName < right.systemName;
    });
}

std::vector<NetworkInterfaceInfo> listNetworkInterfaces()
{
    const std::string refreshedAt = refreshTimestamp();
    std::vector<NetworkInterfaceInfo> output;

#if defined(__linux__)
    ifaddrs* rawInterfaces = nullptr;
    if (getifaddrs(&rawInterfaces) != 0 || rawInterfaces == nullptr) {
        return output;
    }

    std::map<std::string, NetworkInterfaceInfo> byName;
    for (const ifaddrs* entry = rawInterfaces; entry != nullptr; entry = entry->ifa_next) {
        if (entry->ifa_name == nullptr) {
            continue;
        }

        auto& interfaceInfo = byName[entry->ifa_name];
        interfaceInfo.systemName = entry->ifa_name;
        interfaceInfo.displayName = entry->ifa_name;
        interfaceInfo.refreshedAt = refreshedAt;
        interfaceInfo.isUp = (entry->ifa_flags & IFF_UP) != 0;
        interfaceInfo.isRunning = (entry->ifa_flags & IFF_RUNNING) != 0;
        interfaceInfo.isLoopback = (entry->ifa_flags & IFF_LOOPBACK) != 0;
        interfaceInfo.isVirtual = isVirtualInterfaceName(interfaceInfo.systemName);

        if (entry->ifa_addr == nullptr) {
            continue;
        }

        if (entry->ifa_addr->sa_family == AF_PACKET) {
            const auto* linkAddress = reinterpret_cast<const sockaddr_ll*>(entry->ifa_addr);
            const auto macAddress = formatMacAddress(linkAddress);
            if (!macAddress.empty()) {
                interfaceInfo.macAddress = macAddress;
            }
            continue;
        }

        if (entry->ifa_addr->sa_family == AF_INET || entry->ifa_addr->sa_family == AF_INET6) {
            NetworkInterfaceAddress address;
            address.address = numericAddress(entry->ifa_addr);
            address.ipv6 = entry->ifa_addr->sa_family == AF_INET6;
            address.prefixLength = address.ipv6
                ? ipv6PrefixLength(entry->ifa_netmask)
                : ipv4PrefixLength(entry->ifa_netmask);
            if (!address.address.empty()) {
                interfaceInfo.addresses.push_back(std::move(address));
            }
        }
    }

    freeifaddrs(rawInterfaces);

    output.reserve(byName.size());
    for (auto& item : byName) {
        applyBackendDiagnostics(item.second);
        output.push_back(std::move(item.second));
    }
#else
    (void)refreshedAt;
#endif

    sortNetworkInterfacesForDisplay(output);
    return output;
}

} // namespace asset_discovery::capture
