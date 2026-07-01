#include "pnad/capture/AfPacketCaptureBackend.hpp"
#include "pnad/capture/PacketCapture.hpp"

#include <iostream>
#include <string>

namespace {

using asset_discovery::capture::CaptureBackendSelection;
using asset_discovery::capture::captureBackendSelectionName;
using asset_discovery::capture::createCaptureBackend;
using asset_discovery::capture::parseCaptureBackendSelection;

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
    expect(parseCaptureBackendSelection("af-packet") == CaptureBackendSelection::AfPacket, "af-packet should parse");
    expect(!parseCaptureBackendSelection("raw").has_value(), "unknown backend should not parse");
    expect(captureBackendSelectionName(CaptureBackendSelection::AfPacket) == "af-packet",
        "backend name formatter should preserve CLI spelling");
}

void explicitAfPacketEitherCreatesOrExplainsUnavailability()
{
    const auto result = createCaptureBackend(CaptureBackendSelection::AfPacket);
    if (result.backend) {
        expect(result.backend->backendName() == "af-packet", "explicit af-packet should create af-packet backend");
        expect(!result.error.has_value(), "created af-packet backend should not also report an error");
    } else {
        expect(result.error.has_value(), "unavailable explicit af-packet should report an error");
        expect(result.error->find("AF_PACKET") != std::string::npos
                || result.error->find("CAP_NET_RAW") != std::string::npos,
            "af-packet error should explain platform or capability requirement");
    }
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

} // namespace

int main()
{
    parsesBackendNames();
    explicitAfPacketEitherCreatesOrExplainsUnavailability();
    autoSelectionReportsRequest();

    if (failures > 0) {
        std::cerr << failures << " capture backend test expectation(s) failed\n";
        return 1;
    }
    return 0;
}
