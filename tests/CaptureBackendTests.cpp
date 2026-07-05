#include "pnad/capture/NetworkInterface.hpp"
#include "pnad/capture/PacketCapture.hpp"

#include <iostream>
#include <string>
#include <set>
#include <vector>

namespace {

using asset_discovery::capture::CaptureBackendSelection;
using asset_discovery::capture::NetworkInterfaceInfo;
using asset_discovery::capture::captureBackendSelectionName;
using asset_discovery::capture::createCaptureBackend;
using asset_discovery::capture::listNetworkInterfaces;
using asset_discovery::capture::parseCaptureBackendSelection;
using asset_discovery::capture::sortNetworkInterfacesForDisplay;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void parsesBackendNames()
{
    expect(parseCaptureBackendSelection("auto") == CaptureBackendSelection::Auto, "auto should parse");
    expect(parseCaptureBackendSelection("pcap") == CaptureBackendSelection::Pcap, "pcap should parse");
    expect(!parseCaptureBackendSelection("af-packet").has_value(), "af-packet should not parse after removal");
    expect(!parseCaptureBackendSelection("raw").has_value(), "unknown backend should not parse");
    expect(captureBackendSelectionName(CaptureBackendSelection::Pcap) == "pcap",
        "backend name formatter should return pcap");
}

void autoSelectionReportsRequest()
{
    const auto result = createCaptureBackend(CaptureBackendSelection::Auto);
    expect(result.initialStats.requestedBackend == "auto", "auto factory should report requested backend");
    if (result.backend) {
        expect(!result.initialStats.selectedBackend.empty(), "auto factory should report selected backend");
    } else {
        expect(result.error.has_value(), "auto factory should report an error when no backend exists");
    }
}

void interfaceSortingPrefersUsablePhysicalAdapters()
{
    NetworkInterfaceInfo loopback;
    loopback.systemName = "lo";
    loopback.isUp = true;
    loopback.isRunning = true;
    loopback.isLoopback = true;
    loopback.captureAllowed = false;

    NetworkInterfaceInfo down;
    down.systemName = "eth9";
    down.isUp = false;
    down.isRunning = false;
    down.captureAllowed = false;

    NetworkInterfaceInfo active;
    active.systemName = "eth0";
    active.isUp = true;
    active.isRunning = true;
    active.captureAllowed = true;
    active.addresses.push_back({"192.168.1.10", 24, false});

    std::vector<NetworkInterfaceInfo> interfaces = {loopback, down, active};
    sortNetworkInterfacesForDisplay(interfaces);

    expect(interfaces.front().systemName == "eth0", "active non-loopback interface should be sorted first");
    expect(interfaces.back().systemName == "eth9", "down interface should be sorted last");
}

void enumeratesInterfacesWithStableShape()
{
    const auto interfaces = listNetworkInterfaces();
    std::set<std::string> names;
    for (const auto& interfaceInfo : interfaces) {
        expect(!interfaceInfo.systemName.empty(), "enumerated interface should have a system name");
        expect(!interfaceInfo.displayName.empty(), "enumerated interface should have a display name");
        expect(!interfaceInfo.refreshedAt.empty(), "enumerated interface should have a refresh timestamp");
        expect(names.insert(interfaceInfo.systemName).second, "enumerated interface names should be unique");
    }
}

} // namespace

int main()
{
    parsesBackendNames();
    autoSelectionReportsRequest();
    interfaceSortingPrefersUsablePhysicalAdapters();
    enumeratesInterfacesWithStableShape();

    if (failures > 0) {
        std::cerr << failures << " capture backend test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
