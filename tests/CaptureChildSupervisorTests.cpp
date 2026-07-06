#include "pnad/capture/CaptureChildSupervisor.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <thread>

int main()
{
    asset_discovery::capture::CaptureChildSupervisor supervisor;

    const auto started = supervisor.start({
        "/bin/sh",
        {"-c", "printf ready; sleep 5"}
    });
    assert(started.started);
    assert(!started.error.has_value());

    std::string output;
    for (int attempt = 0; attempt < 20 && output.find("ready") == std::string::npos; ++attempt) {
        output += supervisor.readStdout();
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    assert(output.find("ready") != std::string::npos);
    assert(supervisor.isRunning());
    assert(supervisor.stop());
    assert(!supervisor.isRunning());
    assert(supervisor.exitCode().has_value());

    const auto missing = supervisor.start({
        "/definitely/missing/asset-capture-child",
        {}
    });
    assert(missing.started);
    for (int attempt = 0; attempt < 20 && supervisor.isRunning(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    assert(supervisor.exitCode().has_value());

    return 0;
}
