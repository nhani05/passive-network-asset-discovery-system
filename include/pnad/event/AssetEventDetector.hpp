#pragma once

#include "pnad/event/AssetEvent.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace asset_discovery::asset {

struct Ipv4Network {
    std::uint32_t address = 0;
    std::uint32_t mask = 0;
    std::string text;

    bool contains(std::uint32_t ip) const;
    bool contains(std::string_view ip) const;
};

std::optional<std::uint32_t> parseIpv4Address(std::string_view value);
std::optional<Ipv4Network> parseIpv4Network(std::string_view value);

struct AssetEventDetectorConfig {
    std::string interfaceName;
    std::int64_t flipFlopWindowSeconds = 300;
    std::int64_t reappearanceThresholdSeconds = 15552000;
    std::vector<Ipv4Network> localNetworks;
    std::vector<Ipv4Network> ignoredNetworks;
};

class AssetEventDetector {
public:
    explicit AssetEventDetector(AssetEventDetectorConfig config = {});

    std::vector<AssetEvent> detectAndRemember(const parser::AssetObservation& observation);

private:
    struct MacState {
        parser::ObservationTimestamp firstSeen;
        parser::ObservationTimestamp lastSeen;
    };

    struct PreviousMacState {
        std::string macAddress;
        parser::ObservationTimestamp transitionTime;
    };

    AssetEvent baseEvent(
        const parser::AssetObservation& observation,
        AssetEventType type,
        AssetEventSeverity severity,
        std::string message) const;

    bool isIgnoredIp(const std::string& ipAddress) const;
    bool isLocalIp(const std::string& ipAddress) const;
    void rememberObservation(const parser::AssetObservation& observation, const std::string& macAddress);

    AssetEventDetectorConfig config_;
    std::map<std::string, MacState> knownMacs_;
    std::map<std::string, std::string> currentMacByIp_;
    std::map<std::string, std::string> currentIpByMac_;
    std::map<std::string, PreviousMacState> previousMacByIp_;
    std::map<std::string, parser::ObservationTimestamp> lastSeenByPair_;
    std::map<std::string, std::string> hostnameByMac_;
};

std::string normalizeMacAddress(std::string macAddress);
std::int64_t secondsBetween(
    const parser::ObservationTimestamp& earlier,
    const parser::ObservationTimestamp& later);

} // namespace asset_discovery::asset
