#include "pnad/capture/CaptureChildSupervisor.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace asset_discovery::capture {
namespace {

std::string errnoMessage(const std::string& prefix)
{
    return prefix + ": " + std::strerror(errno);
}

bool setNonBlocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

std::optional<int> waitForExit(int pid, bool blocking)
{
    int status = 0;
    const int options = blocking ? 0 : WNOHANG;
    const int result = waitpid(pid, &status, options);
    if (result == 0) {
        return std::nullopt;
    }
    if (result < 0) {
        if (errno == ECHILD) {
            return 0;
        }
        return std::nullopt;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return std::nullopt;
}

} // namespace

CaptureChildSupervisor::~CaptureChildSupervisor()
{
    stop();
    closePipes();
}

CaptureChildStartResult CaptureChildSupervisor::start(const CaptureChildConfig& config)
{
    if (isRunning()) {
        return {false, "capture child is already running"};
    }
    if (config.executablePath.empty()) {
        return {false, "capture child executable path is required"};
    }

    int stdoutPipe[2] = {-1, -1};
    int stderrPipe[2] = {-1, -1};
    if (pipe(stdoutPipe) != 0) {
        return {false, errnoMessage("stdout pipe failed")};
    }
    if (pipe(stderrPipe) != 0) {
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        return {false, errnoMessage("stderr pipe failed")};
    }

    const int pid = fork();
    if (pid < 0) {
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[0]);
        close(stderrPipe[1]);
        return {false, errnoMessage("fork failed")};
    }

    if (pid == 0) {
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[0]);
        close(stderrPipe[1]);

        std::vector<std::string> argvStorage;
        argvStorage.reserve(config.arguments.size() + 1);
        argvStorage.push_back(config.executablePath);
        for (const auto& argument : config.arguments) {
            argvStorage.push_back(argument);
        }

        std::vector<char*> argv;
        argv.reserve(argvStorage.size() + 1);
        for (auto& argument : argvStorage) {
            argv.push_back(argument.data());
        }
        argv.push_back(nullptr);

        execvp(config.executablePath.c_str(), argv.data());
        _exit(127);
    }

    close(stdoutPipe[1]);
    close(stderrPipe[1]);
    stdoutFd_ = stdoutPipe[0];
    stderrFd_ = stderrPipe[0];
    setNonBlocking(stdoutFd_);
    setNonBlocking(stderrFd_);
    childPid_ = pid;
    exitCode_.reset();
    return {true, std::nullopt};
}

bool CaptureChildSupervisor::stop()
{
    if (childPid_ <= 0) {
        return true;
    }

    if (const auto code = waitForExit(childPid_, false); code.has_value()) {
        exitCode_ = *code;
        childPid_ = -1;
        return true;
    }

    (void)kill(childPid_, SIGTERM);
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (const auto code = waitForExit(childPid_, false); code.has_value()) {
            exitCode_ = *code;
            childPid_ = -1;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    (void)kill(childPid_, SIGKILL);
    if (const auto code = waitForExit(childPid_, true); code.has_value()) {
        exitCode_ = *code;
    }
    childPid_ = -1;
    return true;
}

bool CaptureChildSupervisor::isRunning()
{
    if (childPid_ <= 0) {
        return false;
    }
    if (const auto code = waitForExit(childPid_, false); code.has_value()) {
        exitCode_ = *code;
        childPid_ = -1;
        return false;
    }
    return true;
}

std::optional<int> CaptureChildSupervisor::exitCode()
{
    (void)isRunning();
    return exitCode_;
}

std::string CaptureChildSupervisor::readStdout()
{
    return readAvailable(stdoutFd_);
}

std::string CaptureChildSupervisor::readStderr()
{
    return readAvailable(stderrFd_);
}

void CaptureChildSupervisor::closePipes()
{
    if (stdoutFd_ >= 0) {
        close(stdoutFd_);
        stdoutFd_ = -1;
    }
    if (stderrFd_ >= 0) {
        close(stderrFd_);
        stderrFd_ = -1;
    }
}

std::string CaptureChildSupervisor::readAvailable(int fd)
{
    if (fd < 0) {
        return {};
    }

    std::string output;
    char buffer[4096];
    while (true) {
        const auto count = read(fd, buffer, sizeof(buffer));
        if (count > 0) {
            output.append(buffer, buffer + count);
            continue;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }
    return output;
}

} // namespace asset_discovery::capture
