#pragma once

#include <cstddef>

namespace asset_discovery::constants::capture {

inline constexpr const char* DefaultPacketFilter = "arp or udp port 67 or udp port 68";
inline constexpr const char* PcapInterfaceName = "pcap";
inline constexpr const char* BackendAutoName = "auto";
inline constexpr const char* BackendPcapName = "pcap";
inline constexpr const char* LinkTypeEthernetName = "ethernet";
inline constexpr const char* LinkTypeUnknownName = "unknown";
inline constexpr const char* PcapFileExtension = "pcap";
inline constexpr const char* PcapNgFileExtension = "pcapng";

inline constexpr int LiveSnapLength = 65535;
inline constexpr int LivePromiscuousMode = 1;
inline constexpr int LiveReadTimeoutMs = 1000;
inline constexpr int LiveIdleSleepMs = 100;

inline constexpr std::size_t DefaultPacketBatchSize = 128;
inline constexpr std::size_t DefaultQueueCapacity = 1024;

} // namespace asset_discovery::constants::capture
