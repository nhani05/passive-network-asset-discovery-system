#pragma once

#include <cstdint>

namespace asset_discovery::constants::backend {

inline constexpr const char* DefaultListenAddress = "127.0.0.1";
inline constexpr std::uint16_t DefaultPort = 8080;
inline constexpr const char* DefaultRuntimeLogPath = "logs/pnad-runtime.log";

inline constexpr const char* CaptureModeLive = "live";
inline constexpr const char* CaptureModePcap = "pcap";

inline constexpr const char* MethodGet = "GET";
inline constexpr const char* MethodPost = "POST";

inline constexpr const char* StatusPath = "/api/v1/status";
inline constexpr const char* AssetsPath = "/api/v1/assets";
inline constexpr const char* EventsPath = "/api/v1/events";
inline constexpr const char* LogsPath = "/api/v1/logs";
inline constexpr const char* MetricsPath = "/api/v1/metrics";
inline constexpr const char* CaptureStartPath = "/api/v1/capture/start";
inline constexpr const char* CaptureStopPath = "/api/v1/capture/stop";
inline constexpr const char* CaptureRestartPath = "/api/v1/capture/restart";
inline constexpr const char* EventsWebSocketPath = "/ws/events";

inline constexpr int DefaultQueryLimit = 100;
inline constexpr int DefaultAssetQueryLimit = 1000;
inline constexpr int DefaultEventBusCapacity = 1000;
inline constexpr int SocketBacklog = 16;
inline constexpr int HttpRequestBufferSize = 8192;
inline constexpr const char* WebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

} // namespace asset_discovery::constants::backend
