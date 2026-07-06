#pragma once

#include <optional>
#include <string>
#include <vector>

namespace asset_discovery::capture {

struct CaptureChildConfig {
    std::string executablePath;
    std::vector<std::string> arguments;
};

struct CaptureChildStartResult {
    bool started = false;
    std::optional<std::string> error;
};

class CaptureChildSupervisor {
public:
    CaptureChildSupervisor() = default;
    ~CaptureChildSupervisor();

    CaptureChildSupervisor(const CaptureChildSupervisor&) = delete;
    CaptureChildSupervisor& operator=(const CaptureChildSupervisor&) = delete;

    CaptureChildStartResult start(const CaptureChildConfig& config);
    bool stop();
    bool isRunning();
    std::optional<int> exitCode();
    std::string readStdout();
    std::string readStderr();

private:
    void closePipes();
    std::string readAvailable(int fd);

    int childPid_ = -1;
    int stdoutFd_ = -1;
    int stderrFd_ = -1;
    std::optional<int> exitCode_;
};

} // namespace asset_discovery::capture
