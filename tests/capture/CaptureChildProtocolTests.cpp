#include "pnad/capture/CaptureChildProtocol.hpp"

#include <cassert>

int main()
{
    asset_discovery::capture::CaptureChildMessage original;
    original.type = asset_discovery::capture::CaptureChildMessageType::NewData;
    original.packetCount = 42;
    original.droppedCount = 2;
    original.capturePath = "/tmp/test capture.pcap";
    original.message = "ready\twith escaped data";
    original.fields["backend"] = "pcap";

    const auto serialized = asset_discovery::capture::serializeCaptureChildMessage(original);
    const auto parsed = asset_discovery::capture::parseCaptureChildMessage(serialized);

    assert(parsed.has_value());
    assert(parsed->type == original.type);
    assert(parsed->packetCount == 42);
    assert(parsed->droppedCount == 2);
    assert(parsed->capturePath == original.capturePath);
    assert(parsed->message == original.message);
    assert(parsed->fields.at("backend") == "pcap");
    assert(asset_discovery::capture::captureChildMessageTypeName(original.type) == "new_data");
    assert(asset_discovery::capture::parseCaptureChildMessage("event=unknown\n") == std::nullopt);
    assert(asset_discovery::capture::parseCaptureChildMessage("packets=10\n") == std::nullopt);

    return 0;
}
