#include "pnad/backend/BackendConfig.hpp"
#include "pnad/backend/CaptureService.hpp"
#include "pnad/constants/BackendConstants.hpp"
#include "pnad/backend/EventBus.hpp"
#include "pnad/backend/HttpServer.hpp"
#include "pnad/backend/QueryServices.hpp"
#include "pnad/backend/RestApi.hpp"

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

std::mutex wsClientsMutex;
std::vector<int> wsClients;
std::atomic<bool> wsRunning(true);

std::string domainEventJson(const asset_discovery::backend::DomainEvent& event)
{
    std::ostringstream output;
    output
        << "{"
        << "\"type\":" << asset_discovery::backend::jsonString(event.type) << ','
        << "\"timestamp\":" << asset_discovery::backend::jsonString(event.timestamp) << ','
        << "\"severity\":" << asset_discovery::backend::jsonString(event.severity) << ','
        << "\"data\":";

    std::visit([&](const auto& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, asset_discovery::backend::AssetCreatedEvent>) {
            output << "{"
                   << "\"macAddress\":" << asset_discovery::backend::jsonString(val.asset.macAddress) << ','
                   << "\"ipAddresses\":" << (val.asset.ipAddressesJson.empty() ? "[]" : val.asset.ipAddressesJson) << ','
                   << "\"hostname\":" << asset_discovery::backend::jsonString(val.asset.hostname) << ','
                   << "\"firstSeen\":" << asset_discovery::backend::jsonString(val.asset.firstSeen) << ','
                   << "\"lastSeen\":" << asset_discovery::backend::jsonString(val.asset.lastSeen) << ','
                   << "\"discoverySources\":" << (val.asset.discoverySourcesJson.empty() ? "[]" : val.asset.discoverySourcesJson) << ','
                   << "\"observedMetadata\":" << (val.asset.observedMetadataJson.empty() ? "{}" : val.asset.observedMetadataJson)
                   << "}";
        } else if constexpr (std::is_same_v<T, asset_discovery::backend::AssetUpdatedEvent>) {
            output << "{"
                   << "\"macAddress\":" << asset_discovery::backend::jsonString(val.asset.macAddress) << ','
                   << "\"ipAddresses\":" << (val.asset.ipAddressesJson.empty() ? "[]" : val.asset.ipAddressesJson) << ','
                   << "\"hostname\":" << asset_discovery::backend::jsonString(val.asset.hostname) << ','
                   << "\"firstSeen\":" << asset_discovery::backend::jsonString(val.asset.firstSeen) << ','
                   << "\"lastSeen\":" << asset_discovery::backend::jsonString(val.asset.lastSeen) << ','
                   << "\"discoverySources\":" << (val.asset.discoverySourcesJson.empty() ? "[]" : val.asset.discoverySourcesJson) << ','
                   << "\"observedMetadata\":" << (val.asset.observedMetadataJson.empty() ? "{}" : val.asset.observedMetadataJson)
                   << "}";
        } else if constexpr (std::is_same_v<T, asset_discovery::backend::EventDetectedEvent>) {
            output << "{"
                   << "\"id\":" << val.event.id << ','
                   << "\"eventTime\":" << asset_discovery::backend::jsonString(val.event.eventTime) << ','
                   << "\"eventType\":" << asset_discovery::backend::jsonString(val.event.eventType) << ','
                   << "\"severity\":" << asset_discovery::backend::jsonString(val.event.severity) << ','
                   << "\"ipAddress\":" << asset_discovery::backend::jsonString(val.event.ipAddress) << ','
                   << "\"macAddress\":" << asset_discovery::backend::jsonString(val.event.macAddress) << ','
                   << "\"message\":" << asset_discovery::backend::jsonString(val.event.message) << ','
                   << "\"metadata\":" << (val.event.metadataJson.empty() ? "{}" : val.event.metadataJson)
                   << "}";
        } else if constexpr (std::is_same_v<T, asset_discovery::backend::LogMessageEvent>) {
            output << "{"
                   << "\"message\":" << asset_discovery::backend::jsonString(val.message)
                   << "}";
        } else if constexpr (std::is_same_v<T, asset_discovery::backend::CaptureStartedEvent>) {
            output << "{"
                   << "\"state\":" << asset_discovery::backend::jsonString(val.status.stateName) << ','
                   << "\"running\":" << (val.status.running ? "true" : "false") << ','
                   << "\"mode\":" << asset_discovery::backend::jsonString(val.status.mode) << ','
                   << "\"startCount\":" << val.status.startCount << ','
                   << "\"stopCount\":" << val.status.stopCount;
            if (val.status.source.has_value()) {
                output << ",\"source\":" << asset_discovery::backend::jsonString(*val.status.source);
            }
            if (val.status.lastError.has_value()) {
                output << ",\"lastError\":" << asset_discovery::backend::jsonString(*val.status.lastError);
            }
            output << "}";
        } else if constexpr (std::is_same_v<T, asset_discovery::backend::CaptureStoppedEvent>) {
            output << "{"
                   << "\"state\":" << asset_discovery::backend::jsonString(val.status.stateName) << ','
                   << "\"running\":" << (val.status.running ? "true" : "false") << ','
                   << "\"mode\":" << asset_discovery::backend::jsonString(val.status.mode) << ','
                   << "\"startCount\":" << val.status.startCount << ','
                   << "\"stopCount\":" << val.status.stopCount;
            if (val.status.source.has_value()) {
                output << ",\"source\":" << asset_discovery::backend::jsonString(*val.status.source);
            }
            if (val.status.lastError.has_value()) {
                output << ",\"lastError\":" << asset_discovery::backend::jsonString(*val.status.lastError);
            }
            output << "}";
        } else if constexpr (std::is_same_v<T, asset_discovery::backend::CaptureErrorEvent>) {
            output << "{"
                   << "\"error\":" << asset_discovery::backend::jsonString(val.error)
                   << "}";
        } else if constexpr (std::is_same_v<T, asset_discovery::backend::MetricsUpdatedEvent>) {
            output << "{"
                   << "\"assetCount\":" << val.metrics.assetCount << ','
                   << "\"eventCount\":" << val.metrics.eventCount << ','
                   << "\"captureStarts\":" << val.metrics.captureStarts << ','
                   << "\"captureStops\":" << val.metrics.captureStops
                   << "}";
        }
    }, event.data);

    output << "}";
    return output.str();
}

std::vector<std::uint8_t> formatWebSocketFrame(const std::string& message)
{
    std::vector<std::uint8_t> frame;
    frame.push_back(0x81);
    std::size_t len = message.size();
    if (len <= 125) {
        frame.push_back(static_cast<std::uint8_t>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<std::uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<std::uint8_t>((len >> (i * 8)) & 0xFF));
        }
    }
    frame.insert(frame.end(), message.begin(), message.end());
    return frame;
}

void wsDeliveryLoop(asset_discovery::backend::EventQueuePtr queue)
{
    asset_discovery::backend::DomainEvent event;
    while (wsRunning.load(std::memory_order_relaxed) && queue->waitPop(event)) {
        std::string jsonPayload = domainEventJson(event);
        auto frame = formatWebSocketFrame(jsonPayload);

        std::vector<int> deadClients;
        {
            std::lock_guard<std::mutex> lock(wsClientsMutex);
            for (int clientFd : wsClients) {
                auto sent = ::send(clientFd, frame.data(), frame.size(), MSG_NOSIGNAL);
                if (sent < 0) {
                    deadClients.push_back(clientFd);
                }
            }

            for (int clientFd : deadClients) {
                wsClients.erase(std::remove(wsClients.begin(), wsClients.end(), clientFd), wsClients.end());
                ::close(clientFd);
            }
        }
    }
}

std::string captureStatusJson(const asset_discovery::backend::CaptureServiceStatus& status)
{
    std::ostringstream output;
    output
        << "{"
        << "\"state\":" << asset_discovery::backend::jsonString(status.stateName) << ','
        << "\"running\":" << (status.running ? "true" : "false") << ','
        << "\"mode\":" << asset_discovery::backend::jsonString(status.mode) << ','
        << "\"startCount\":" << status.startCount << ','
        << "\"stopCount\":" << status.stopCount;
    if (status.source.has_value()) {
        output << ",\"source\":" << asset_discovery::backend::jsonString(*status.source);
    }
    if (status.lastError.has_value()) {
        output << ",\"lastError\":" << asset_discovery::backend::jsonString(*status.lastError);
    }
    output << "}";
    return output.str();
}

std::string statusJson(
    const asset_discovery::backend::BackendHealthStatus& health,
    long long uptimeSeconds)
{
    std::ostringstream output;
    output
        << "{"
        << "\"status\":" << asset_discovery::backend::jsonString(health.status) << ','
        << "\"healthy\":" << (health.healthy ? "true" : "false") << ','
        << "\"ready\":true,"
        << "\"uptimeSeconds\":" << uptimeSeconds << ','
        << "\"capture\":" << captureStatusJson(health.capture);
    if (health.storageError.has_value()) {
        output << ",\"storageError\":" << asset_discovery::backend::jsonString(*health.storageError);
    }
    output << "}";
    return output.str();
}

std::string assetsJson(const std::vector<asset_discovery::backend::BackendAssetRecord>& assets)
{
    std::ostringstream output;
    output << "{\"assets\":[";
    bool first = true;
    for (const auto& asset : assets) {
        if (!first) {
            output << ',';
        }
        output
            << "{"
            << "\"macAddress\":" << asset_discovery::backend::jsonString(asset.macAddress) << ','
            << "\"ipAddresses\":" << (asset.ipAddressesJson.empty() ? "[]" : asset.ipAddressesJson) << ','
            << "\"hostname\":" << asset_discovery::backend::jsonString(asset.hostname) << ','
            << "\"firstSeen\":" << asset_discovery::backend::jsonString(asset.firstSeen) << ','
            << "\"lastSeen\":" << asset_discovery::backend::jsonString(asset.lastSeen) << ','
            << "\"discoverySources\":" << (asset.discoverySourcesJson.empty() ? "[]" : asset.discoverySourcesJson) << ','
            << "\"observedMetadata\":" << (asset.observedMetadataJson.empty() ? "{}" : asset.observedMetadataJson)
            << "}";
        first = false;
    }
    output << "]}";
    return output.str();
}

std::string eventsJson(const std::vector<asset_discovery::backend::BackendEventRecord>& events)
{
    std::ostringstream output;
    output << "{\"events\":[";
    bool first = true;
    for (const auto& event : events) {
        if (!first) {
            output << ',';
        }
        output
            << "{"
            << "\"id\":" << event.id << ','
            << "\"eventTime\":" << asset_discovery::backend::jsonString(event.eventTime) << ','
            << "\"eventType\":" << asset_discovery::backend::jsonString(event.eventType) << ','
            << "\"severity\":" << asset_discovery::backend::jsonString(event.severity) << ','
            << "\"ipAddress\":" << asset_discovery::backend::jsonString(event.ipAddress) << ','
            << "\"macAddress\":" << asset_discovery::backend::jsonString(event.macAddress) << ','
            << "\"message\":" << asset_discovery::backend::jsonString(event.message) << ','
            << "\"metadata\":" << (event.metadataJson.empty() ? "{}" : event.metadataJson)
            << "}";
        first = false;
    }
    output << "]}";
    return output.str();
}

std::string logsJson(const std::vector<asset_discovery::backend::BackendLogRecord>& logs)
{
    std::ostringstream output;
    output << "{\"logs\":[";
    bool first = true;
    for (const auto& log : logs) {
        if (!first) {
            output << ',';
        }
        output
            << "{"
            << "\"message\":" << asset_discovery::backend::jsonString(log.message)
            << "}";
        first = false;
    }
    output << "]}";
    return output.str();
}

std::string metricsJson(const asset_discovery::backend::BackendMetricsSnapshot& metrics)
{
    std::ostringstream output;
    output
        << "{"
        << "\"assetCount\":" << metrics.assetCount << ','
        << "\"eventCount\":" << metrics.eventCount << ','
        << "\"captureStarts\":" << metrics.captureStarts << ','
        << "\"captureStops\":" << metrics.captureStops
        << "}";
    return output.str();
}

std::optional<int> parseLimitQuery(const std::string& query, std::string& errorMsg)
{
    if (query.empty()) {
        return std::nullopt;
    }
    std::string limitStr;
    std::istringstream iss(query);
    std::string pair;
    while (std::getline(iss, pair, '&')) {
        auto eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = pair.substr(0, eq);
            std::string val = pair.substr(eq + 1);
            if (key == "limit") {
                limitStr = val;
                break;
            }
        }
    }
    if (limitStr.empty()) {
        return std::nullopt;
    }
    if (limitStr.find_first_not_of("0123456789") != std::string::npos) {
        errorMsg = "limit parameter must be a positive integer";
        return -1;
    }
    try {
        int val = std::stoi(limitStr);
        if (val <= 0) {
            errorMsg = "limit parameter must be greater than zero";
            return -1;
        }
        return val;
    } catch (...) {
        errorMsg = "limit parameter is invalid";
        return -1;
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        arguments.emplace_back(argv[i]);
    }

    const auto parsed = asset_discovery::backend::parseBackendArguments(arguments);
    if (parsed.helpRequested) {
        std::cout << asset_discovery::backend::backendUsage();
        return 0;
    }

    if (parsed.error.has_value()) {
        std::cerr << "assetd: " << *parsed.error << "\n\n"
                  << asset_discovery::backend::backendUsage();
        return 2;
    }

    const auto& config = parsed.config;
    asset_discovery::backend::logBackendMessage(config.runtimeLogPath, "assetd backend service started.");
    asset_discovery::backend::logBackendMessage(config.runtimeLogPath, "status: healthy");
    asset_discovery::backend::logBackendMessage(config.runtimeLogPath, "listen: " + config.listenAddress + ":" + std::to_string(config.port));
    asset_discovery::backend::logBackendMessage(config.runtimeLogPath, "database: " + config.sqlitePath);
    asset_discovery::backend::logBackendMessage(config.runtimeLogPath, "capture_mode: " + asset_discovery::backend::captureModeName(config.captureMode));
    asset_discovery::backend::logBackendMessage(config.runtimeLogPath, "runtime_log: " + config.runtimeLogPath);

    std::cout << "assetd backend service started.\n";
    std::cout << "status: healthy\n";
    std::cout << "listen: " << config.listenAddress << ':' << config.port << '\n';
    std::cout << "database: " << config.sqlitePath << '\n';
    std::cout << "capture_mode: "
              << asset_discovery::backend::captureModeName(config.captureMode)
              << '\n';
    std::cout << "runtime_log: " << config.runtimeLogPath << '\n';
    if (config.serve) {
        const auto startedAt = std::chrono::steady_clock::now();
        asset_discovery::backend::CaptureService captureService(config);
        asset_discovery::backend::HealthService healthService(config.sqlitePath, captureService);
        asset_discovery::backend::AssetQueryService assetQueryService(config.sqlitePath);
        asset_discovery::backend::EventQueryService eventQueryService(config.sqlitePath);
        asset_discovery::backend::LogQueryService logQueryService(config.runtimeLogPath);
        asset_discovery::backend::MetricsService metricsService(config.sqlitePath, captureService);
        asset_discovery::backend::HttpServer server([&](const asset_discovery::backend::HttpRequest& request) {
            if (request.method == asset_discovery::constants::backend::MethodGet
                && request.path == asset_discovery::constants::backend::StatusPath) {
                const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startedAt).count();
                return asset_discovery::backend::jsonSuccess(statusJson(healthService.status(), uptime));
            }
            if (request.method == asset_discovery::constants::backend::MethodGet
                && request.path == asset_discovery::constants::backend::AssetsPath) {
                try {
                    return asset_discovery::backend::jsonSuccess(
                        assetsJson(assetQueryService.listAssets()));
                } catch (const std::exception& error) {
                    return asset_discovery::backend::jsonError(500, "asset_query_failed", error.what());
                }
            }
            if (request.method == asset_discovery::constants::backend::MethodGet
                && request.path == asset_discovery::constants::backend::EventsPath) {
                std::string errorMsg;
                const auto limitOpt = parseLimitQuery(request.query, errorMsg);
                if (limitOpt.has_value() && *limitOpt == -1) {
                    return asset_discovery::backend::jsonError(400, "invalid_limit", errorMsg);
                }
                const int limit = limitOpt.value_or(asset_discovery::constants::backend::DefaultQueryLimit);
                try {
                    return asset_discovery::backend::jsonSuccess(
                        eventsJson(eventQueryService.listEvents(limit)));
                } catch (const std::exception& error) {
                    return asset_discovery::backend::jsonError(500, "event_query_failed", error.what());
                }
            }
            if (request.method == asset_discovery::constants::backend::MethodGet
                && request.path == asset_discovery::constants::backend::LogsPath) {
                std::string errorMsg;
                const auto limitOpt = parseLimitQuery(request.query, errorMsg);
                if (limitOpt.has_value() && *limitOpt == -1) {
                    return asset_discovery::backend::jsonError(400, "invalid_limit", errorMsg);
                }
                const int limit = limitOpt.value_or(asset_discovery::constants::backend::DefaultQueryLimit);
                try {
                    return asset_discovery::backend::jsonSuccess(
                        logsJson(logQueryService.recentLogs(limit)));
                } catch (const std::exception& error) {
                    return asset_discovery::backend::jsonError(500, "log_query_failed", error.what());
                }
            }
            if (request.method == asset_discovery::constants::backend::MethodGet
                && request.path == asset_discovery::constants::backend::MetricsPath) {
                try {
                    return asset_discovery::backend::jsonSuccess(
                        metricsJson(metricsService.snapshot()));
                } catch (const std::exception& error) {
                    return asset_discovery::backend::jsonError(500, "metrics_query_failed", error.what());
                }
            }
            if (request.method == asset_discovery::constants::backend::MethodPost
                && request.path == asset_discovery::constants::backend::CaptureStartPath) {
                const auto result = captureService.start();
                if (result.accepted) {
                    return asset_discovery::backend::jsonSuccess(captureStatusJson(result.status));
                } else {
                    return asset_discovery::backend::jsonError(400, "capture_command_rejected", result.error.value_or("failed to start capture"));
                }
            }
            if (request.method == asset_discovery::constants::backend::MethodPost
                && request.path == asset_discovery::constants::backend::CaptureStopPath) {
                const auto result = captureService.stop();
                if (result.accepted) {
                    return asset_discovery::backend::jsonSuccess(captureStatusJson(result.status));
                } else {
                    return asset_discovery::backend::jsonError(400, "capture_command_rejected", result.error.value_or("failed to stop capture"));
                }
            }
            if (request.method == asset_discovery::constants::backend::MethodPost
                && request.path == asset_discovery::constants::backend::CaptureRestartPath) {
                const auto result = captureService.restart();
                if (result.accepted) {
                    return asset_discovery::backend::jsonSuccess(captureStatusJson(result.status));
                } else {
                    return asset_discovery::backend::jsonError(400, "capture_command_rejected", result.error.value_or("failed to restart capture"));
                }
            }
            if (request.method == asset_discovery::constants::backend::MethodGet
                && request.path == asset_discovery::constants::backend::EventsWebSocketPath) {
                asset_discovery::backend::HttpResponse response;
                response.status = 101;
                return response;
            }
            return asset_discovery::backend::jsonError(404, "not_found", "endpoint not found");
        }, [&](int clientFd) {
            std::lock_guard<std::mutex> lock(wsClientsMutex);
            wsClients.push_back(clientFd);
        });

        auto wsQueue = asset_discovery::backend::EventBus::rx().subscribe();
        wsRunning.store(true);
        std::thread deliveryThread(wsDeliveryLoop, wsQueue);

        if (const auto error = server.serve(config.listenAddress, config.port); error.has_value()) {
            std::cerr << "assetd: " << *error << '\n';
            asset_discovery::backend::logBackendMessage(config.runtimeLogPath, "assetd: " + *error);
            wsRunning.store(false);
            asset_discovery::backend::EventBus::rx().shutdown();
            if (deliveryThread.joinable()) {
                deliveryThread.join();
            }
            {
                std::lock_guard<std::mutex> lock(wsClientsMutex);
                for (int clientFd : wsClients) {
                    ::close(clientFd);
                }
                wsClients.clear();
            }
            return 1;
        }

        wsRunning.store(false);
        asset_discovery::backend::EventBus::rx().shutdown();
        if (deliveryThread.joinable()) {
            deliveryThread.join();
        }
        {
            std::lock_guard<std::mutex> lock(wsClientsMutex);
            for (int clientFd : wsClients) {
                ::close(clientFd);
            }
            wsClients.clear();
        }
    }
    return 0;
}
