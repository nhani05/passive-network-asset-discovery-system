#include "pnad/event/AssetEventDetector.hpp"

#include "pnad/discovery/AssetStore.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <sstream>
#include <utility>

namespace asset_discovery::asset {
namespace {

constexpr std::uint32_t fullIpv4Mask = 0xffffffffU;

std::optional<int> parseOctet(std::string_view value)
{
    if (value.empty()) {
        return std::nullopt;
    }

    int parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end || parsed < 0 || parsed > 255) {
        return std::nullopt;
    }
    return parsed;
}

std::string networkListText(const std::vector<Ipv4Network>& networks)
{
    std::ostringstream output;
    bool first = true;
    for (const auto& network : networks) {
        if (!first) {
            output << ",";
        }
        output << network.text;
        first = false;
    }
    return output.str();
}

std::optional<std::string> metadataValue(
    const parser::AssetObservation& observation,
    const std::string& key)
{
    const auto found = observation.metadata.find(key);
    if (found == observation.metadata.end()) {
        return std::nullopt;
    }
    return found->second;
}

} // namespace

std::string normalizeMacAddress(std::string macAddress)
{
    std::transform(macAddress.begin(), macAddress.end(), macAddress.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return macAddress;
}

std::int64_t secondsBetween(
    const parser::ObservationTimestamp& earlier,
    const parser::ObservationTimestamp& later)
{
    auto seconds = later.seconds - earlier.seconds;
    if (later.microseconds < earlier.microseconds) {
        --seconds;
    }
    return seconds;
}

std::optional<std::uint32_t> parseIpv4Address(std::string_view value)
{
    std::uint32_t result = 0;
    std::size_t start = 0;
    for (int octetIndex = 0; octetIndex < 4; ++octetIndex) {
        const auto dot = value.find('.', start);
        if ((octetIndex < 3 && dot == std::string_view::npos)
            || (octetIndex == 3 && dot != std::string_view::npos)) {
            return std::nullopt;
        }

        const auto end = octetIndex == 3 ? value.size() : dot;
        const auto octet = parseOctet(value.substr(start, end - start));
        if (!octet.has_value()) {
            return std::nullopt;
        }
        result = (result << 8U) | static_cast<std::uint32_t>(*octet);
        start = end + 1;
    }
    return result;
}

std::optional<Ipv4Network> parseIpv4Network(std::string_view value)
{
    const auto slash = value.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 >= value.size()) {
        return std::nullopt;
    }

    const auto address = parseIpv4Address(value.substr(0, slash));
    if (!address.has_value()) {
        return std::nullopt;
    }

    int prefixLength = 0;
    const auto prefix = value.substr(slash + 1);
    const auto* begin = prefix.data();
    const auto* end = prefix.data() + prefix.size();
    const auto parsed = std::from_chars(begin, end, prefixLength);
    if (parsed.ec != std::errc() || parsed.ptr != end || prefixLength < 0 || prefixLength > 32) {
        return std::nullopt;
    }

    const std::uint32_t mask = prefixLength == 0
        ? 0U
        : static_cast<std::uint32_t>(fullIpv4Mask << (32 - prefixLength));

    Ipv4Network network;
    network.address = *address & mask;
    network.mask = mask;
    network.text = std::string(value);
    return network;
}

bool Ipv4Network::contains(std::uint32_t ip) const
{
    return (ip & mask) == address;
}

bool Ipv4Network::contains(std::string_view ip) const
{
    const auto parsed = parseIpv4Address(ip);
    return parsed.has_value() && contains(*parsed);
}

AssetEventDetector::AssetEventDetector(AssetEventDetectorConfig config)
    : config_(std::move(config))
{
}

AssetEvent AssetEventDetector::baseEvent(
    const parser::AssetObservation& observation,
    AssetEventType type,
    AssetEventSeverity severity,
    std::string message) const
{
    AssetEvent event;
    event.timestamp = observation.timestamp;
    event.type = type;
    event.severity = severity;
    event.ipAddress = observation.ipAddress;
    event.macAddress = normalizeMacAddress(observation.macAddress);
    event.hostname = observation.hostname;
    event.protocol = observation.sourceId;
    event.interfaceName = config_.interfaceName;
    event.message = std::move(message);
    return event;
}

bool AssetEventDetector::isIgnoredIp(const std::string& ipAddress) const
{
    return std::any_of(config_.ignoredNetworks.begin(), config_.ignoredNetworks.end(), [&](const auto& network) {
        return network.contains(ipAddress);
    });
}

bool AssetEventDetector::isLocalIp(const std::string& ipAddress) const
{
    if (config_.localNetworks.empty()) {
        return true;
    }
    return std::any_of(config_.localNetworks.begin(), config_.localNetworks.end(), [&](const auto& network) {
        return network.contains(ipAddress);
    });
}

void AssetEventDetector::rememberObservation(
    const parser::AssetObservation& observation,
    const std::string& macAddress)
{
    auto known = knownMacs_.find(macAddress);
    if (known == knownMacs_.end()) {
        knownMacs_[macAddress] = {observation.timestamp, observation.timestamp};
    } else {
        if (timestampLess(observation.timestamp, known->second.firstSeen)) {
            known->second.firstSeen = observation.timestamp;
        }
        if (timestampLess(known->second.lastSeen, observation.timestamp)) {
            known->second.lastSeen = observation.timestamp;
        }
    }

    if (observation.ipAddress.has_value()) {
        currentIpByMac_[macAddress] = *observation.ipAddress;
        currentMacByIp_[*observation.ipAddress] = macAddress;
        lastSeenByPair_[*observation.ipAddress + "|" + macAddress] = observation.timestamp;
    }

    if (observation.hostname.has_value() && !observation.hostname->empty()) {
        hostnameByMac_[macAddress] = *observation.hostname;
    }
}

std::vector<AssetEvent> AssetEventDetector::detectAndRemember(
    const parser::AssetObservation& observation)
{
    std::vector<AssetEvent> events;
    if (observation.macAddress.empty()) {
        return events;
    }

    const auto macAddress = normalizeMacAddress(observation.macAddress);
    const auto knownMac = knownMacs_.find(macAddress);
    const bool isNewMac = knownMac == knownMacs_.end();

    if (isNewMac) {
        events.push_back(baseEvent(
            observation,
            AssetEventType::NewAsset,
            AssetEventSeverity::Info,
            "New asset discovered"));
    }

    const auto ethernetSourceMac = metadataValue(observation, "ethernet.source_mac");
    const auto arpSenderMac = metadataValue(observation, "arp.sender_mac");
    if (observation.sourceId == parser::sourceIdArp
        && ethernetSourceMac.has_value()
        && arpSenderMac.has_value()
        && normalizeMacAddress(*ethernetSourceMac) != normalizeMacAddress(*arpSenderMac)) {
        auto event = baseEvent(
            observation,
            AssetEventType::EthernetArpMacMismatch,
            AssetEventSeverity::High,
            "Ethernet source MAC does not match ARP sender MAC");
        event.metadata["ethernet_source_mac"] = normalizeMacAddress(*ethernetSourceMac);
        event.metadata["arp_sender_mac"] = normalizeMacAddress(*arpSenderMac);
        events.push_back(std::move(event));
    }

    if (observation.ipAddress.has_value()) {
        const auto& ipAddress = *observation.ipAddress;
        if (!config_.localNetworks.empty() && !isIgnoredIp(ipAddress) && !isLocalIp(ipAddress)) {
            auto event = baseEvent(
                observation,
                AssetEventType::NonLocalSourceIp,
                AssetEventSeverity::Warning,
                "Source IP is not local to configured subnet");
            event.metadata["local_networks"] = networkListText(config_.localNetworks);
            events.push_back(std::move(event));
        }

        const auto currentMac = currentMacByIp_.find(ipAddress);
        if (currentMac != currentMacByIp_.end() && currentMac->second != macAddress) {
            auto event = baseEvent(
                observation,
                AssetEventType::MacChangedForIp,
                AssetEventSeverity::Warning,
                "IP address is now associated with a different MAC address");
            event.oldMacAddress = currentMac->second;
            event.newMacAddress = macAddress;
            events.push_back(std::move(event));

            const auto previousMac = previousMacByIp_.find(ipAddress);
            if (previousMac != previousMacByIp_.end()
                && previousMac->second.macAddress == macAddress
                && secondsBetween(previousMac->second.transitionTime, observation.timestamp)
                    <= config_.flipFlopWindowSeconds) {
                auto flipFlopEvent = baseEvent(
                    observation,
                    AssetEventType::IpMacFlipFlop,
                    AssetEventSeverity::High,
                    "IP/MAC mapping is flipping between two MAC addresses");
                flipFlopEvent.oldMacAddress = currentMac->second;
                flipFlopEvent.newMacAddress = macAddress;
                flipFlopEvent.metadata["mac_a"] = macAddress;
                flipFlopEvent.metadata["mac_b"] = currentMac->second;
                events.push_back(std::move(flipFlopEvent));
            }
            previousMacByIp_[ipAddress] = {currentMac->second, observation.timestamp};
        }

        const auto currentIp = currentIpByMac_.find(macAddress);
        if (currentIp != currentIpByMac_.end() && currentIp->second != ipAddress) {
            auto event = baseEvent(
                observation,
                AssetEventType::IpChangedForMac,
                AssetEventSeverity::Info,
                "MAC address is now associated with a different IP address");
            event.oldIpAddress = currentIp->second;
            event.newIpAddress = ipAddress;
            events.push_back(std::move(event));
        }

        const auto pairKey = ipAddress + "|" + macAddress;
        const auto lastSeen = lastSeenByPair_.find(pairKey);
        if (lastSeen != lastSeenByPair_.end()
            && secondsBetween(lastSeen->second, observation.timestamp)
                >= config_.reappearanceThresholdSeconds) {
            auto event = baseEvent(
                observation,
                AssetEventType::AssetReappeared,
                AssetEventSeverity::Info,
                "Asset reappeared after inactivity period");
            event.metadata["last_seen_before"] = formatEventTimestamp(lastSeen->second);
            events.push_back(std::move(event));
        }
    }

    if (observation.hostname.has_value() && !observation.hostname->empty()) {
        const auto knownHostname = hostnameByMac_.find(macAddress);
        if (knownHostname == hostnameByMac_.end()) {
            events.push_back(baseEvent(
                observation,
                AssetEventType::HostnameLearned,
                AssetEventSeverity::Info,
                "Hostname learned for asset"));
        } else if (knownHostname->second != *observation.hostname) {
            auto event = baseEvent(
                observation,
                AssetEventType::HostnameChanged,
                AssetEventSeverity::Warning,
                "Hostname changed for asset");
            event.metadata["old_hostname"] = knownHostname->second;
            event.metadata["new_hostname"] = *observation.hostname;
            events.push_back(std::move(event));
        }
    }

    rememberObservation(observation, macAddress);
    return events;
}

} // namespace asset_discovery::asset
