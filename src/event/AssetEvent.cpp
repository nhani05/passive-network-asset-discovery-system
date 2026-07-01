#include "pnad/event/AssetEvent.hpp"

#include <sstream>

namespace asset_discovery::asset {

std::string assetEventTypeName(AssetEventType type)
{
    switch (type) {
    case AssetEventType::NewAsset:
        return "new_asset";
    case AssetEventType::AssetSeen:
        return "asset_seen";
    case AssetEventType::MacChangedForIp:
        return "mac_changed_for_ip";
    case AssetEventType::IpChangedForMac:
        return "ip_changed_for_mac";
    case AssetEventType::IpMacFlipFlop:
        return "ip_mac_flip_flop";
    case AssetEventType::AssetReappeared:
        return "asset_reappeared";
    case AssetEventType::HostnameLearned:
        return "hostname_learned";
    case AssetEventType::HostnameChanged:
        return "hostname_changed";
    case AssetEventType::EthernetArpMacMismatch:
        return "ethernet_arp_mac_mismatch";
    case AssetEventType::NonLocalSourceIp:
        return "non_local_source_ip";
    }
    return "asset_seen";
}

std::string assetEventSeverityName(AssetEventSeverity severity)
{
    switch (severity) {
    case AssetEventSeverity::Debug:
        return "debug";
    case AssetEventSeverity::Info:
        return "info";
    case AssetEventSeverity::Warning:
        return "warning";
    case AssetEventSeverity::High:
        return "high";
    }
    return "info";
}

std::string assetEventSeverityLabel(AssetEventSeverity severity)
{
    switch (severity) {
    case AssetEventSeverity::Debug:
        return "DEBUG";
    case AssetEventSeverity::Info:
        return "INFO";
    case AssetEventSeverity::Warning:
        return "WARN";
    case AssetEventSeverity::High:
        return "HIGH";
    }
    return "INFO";
}

std::string formatEventTimestamp(const parser::ObservationTimestamp& timestamp)
{
    std::ostringstream output;
    output << timestamp.seconds << "." << timestamp.microseconds;
    return output.str();
}

} // namespace asset_discovery::asset
