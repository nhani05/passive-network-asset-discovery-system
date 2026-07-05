#include "pnad/gui/CaptureController.hpp"
#include "pnad/app/LiveCapturePipeline.hpp"
#include "pnad/discovery/AssetMonitor.hpp"
#include "pnad/capture/PacketCapture.hpp"
#include "pnad/packet/PacketParserFacade.hpp"
#include "pnad/storage/SQLiteWriter.hpp"
#include "pnad/event/EventSink.hpp"
#include "pnad/gui/DesktopRunConfig.hpp"

#include <QDebug>
#include <QFileDialog>
#include <stdexcept>
#include <sstream>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <unistd.h>
#include <limits.h>

namespace asset_discovery::gui {
namespace {

parser::ObservationTimestamp toObservationTimestamp(
    const capture::PacketTimestamp& timestamp)
{
    return {timestamp.seconds, timestamp.microseconds};
}

std::string escapeJsonString(const std::string& value)
{
    std::string output = "";
    for (const char character : value) {
        switch (character) {
        case '\\': output += "\\\\"; break;
        case '"':  output += "\\\""; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:   output += character; break;
        }
    }
    return output;
}

std::string mapToJson(const std::map<std::string, std::string>& metadata)
{
    std::ostringstream output;
    output << "{";
    bool first = true;
    for (const auto& item : metadata) {
        if (!first) {
            output << ",";
        }
        output << "\"" << escapeJsonString(item.first) << "\":\"" << escapeJsonString(item.second) << "\"";
        first = false;
    }
    output << "}";
    return output.str();
}

std::string getExecutablePath()
{
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return "asset-discovery-gui";
}

} // namespace

CaptureController::CaptureController(QObject* parent)
    : QObject(parent)
{
    loadDefaults();
    loadSettingsFromDb();
}

CaptureController::~CaptureController()
{
    stopCapture();
}

void CaptureController::loadDefaults()
{
    interfaceName_ = "";
    pcapPath_ = "";
    configPath_ = "";
    profileName_ = "";
    packetFilter_ = "arp or udp port 67 or udp port 68";
    captureBackend_ = "auto";
    outputFormat_ = "json";
    isLive_ = true;
    eventRateLimitSeconds_ = 60;
    eventQueueCapacity_ = 1024;
    flipFlopWindowSeconds_ = 300;
    reappearanceThresholdSeconds_ = 15552000;
    localNetworks_.clear();
    ignoredNetworks_ = QStringList() << "127.0.0.0/8" << "169.254.0.0/16";

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = ".";
    } else {
        QDir().mkpath(dataDir);
    }
    sqlitePath_ = dataDir + "/pnad.db";

    statusText_ = "Stopped";
    lastError_ = "";
    validationError_ = "";
    recentFailureSummary_ = "";
    runtimeLogPath_ = "logs/pnad-runtime.log";

    emit interfaceNameChanged();
    emit pcapPathChanged();
    emit configPathChanged();
    emit profileNameChanged();
    emit packetFilterChanged();
    emit captureBackendChanged();
    emit outputFormatChanged();
    emit isLiveChanged();
    emit eventRateLimitSecondsChanged();
    emit eventQueueCapacityChanged();
    emit flipFlopWindowSecondsChanged();
    emit reappearanceThresholdSecondsChanged();
    emit localNetworksChanged();
    emit ignoredNetworksChanged();
    emit sqlitePathChanged();
    emit statusTextChanged();
    emit lastErrorChanged();
    emit validationErrorChanged();
    emit recentFailureSummaryChanged();
    emit runtimeLogPathChanged();
}

void CaptureController::startCapture()
{
    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        if (isRunning_) {
            return;
        }
    }

    if (!validateSettings() || (!isLive_ && !validatePcapSource())) {
        statusText_ = "Configuration error";
        emit statusTextChanged();
        return;
    }
    saveSettingsToDb();

    try {
        storage::SQLiteWriter writer(sqlitePath_.toStdString());
        storage::AnalysisSessionRecord session;
        session.mode = isLive_ ? "Live Capture" : "PCAP Analysis";
        session.source = isLive_
            ? interfaceName_.trimmed().toStdString()
            : pcapPath_.trimmed().toStdString();
        session.status = "Running";
        session.storageContext = sqlitePath_.toStdString();
        if (const auto error = writer.createAnalysisSession(session); error.has_value()) {
            setValidationError(QString::fromStdString(*error));
            statusText_ = "Session error";
            emit statusTextChanged();
            return;
        }
        activeSessionId_ = session.id;
    } catch (const std::exception& e) {
        setValidationError(QString::fromStdString(e.what()));
        statusText_ = "Session error";
        emit statusTextChanged();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        stopRequested_ = false;
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
        workerThread_ = std::thread(&CaptureController::runCaptureWorker, this);
    }
}

void CaptureController::startLiveCapture()
{
    setIsLive(true);
    startCapture();
}

void CaptureController::startPcapAnalysis()
{
    setIsLive(false);
    startCapture();
}

void CaptureController::stopCapture()
{
    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        stopRequested_ = true;
    }

    if (workerThread_.joinable()
        && std::this_thread::get_id() != workerThread_.get_id()) {
        workerThread_.join();
    }
}

void CaptureController::runCaptureWorker()
{
    isRunning_ = true;
    emit isRunningChanged();
    statusText_ = "Running";
    emit statusTextChanged();

    try {
        const auto configResult = buildCurrentConfig();
        if (configResult.error.has_value()) {
            throw std::runtime_error(*configResult.error);
        }
        const auto appConfig = configResult.config;

        // Initialize SQLite Writer for background EventSink pipeline
        auto dispatcher = std::make_unique<output::EventDispatcher>();
        dispatcher->addSink(std::make_unique<storage::SQLiteWriter>(*appConfig.database.sqlitePath));

        if (isLive_) {
            auto backendResult = capture::createCaptureBackend(appConfig.capture.backend);
            if (backendResult.error.has_value() || !backendResult.backend) {
                throw std::runtime_error(backendResult.error.value_or("Capture backend could not be created"));
            }

            capture::LiveCaptureOptions liveOptions;
            liveOptions.stopRequested = [this]() {
                return stopRequested_.load();
            };

            capture::CaptureConfig captureConfig;
            captureConfig.interfaceName = *appConfig.capture.interfaceName;
            captureConfig.liveOptions = liveOptions;
            captureConfig.packetFilter = appConfig.capture.packetFilter;
            captureConfig.requestedBackend = appConfig.capture.backend;

            live::LivePipelineOptions livePipelineOptions;
            monitor::AssetMonitorConfig monitorConfig;
            monitorConfig.detector.interfaceName = appConfig.capture.interfaceName.value_or("");
            monitorConfig.detector.localNetworks = appConfig.network.localNetworks;
            monitorConfig.detector.ignoredNetworks = appConfig.network.ignoredNetworks;
            monitorConfig.detector.flipFlopWindowSeconds = appConfig.events.flipFlopWindowSeconds;
            monitorConfig.detector.reappearanceThresholdSeconds = appConfig.events.reappearanceThresholdSeconds;
            monitorConfig.eventRateLimitSeconds = appConfig.events.rateLimitSeconds;

            livePipelineOptions.monitorConfig = std::move(monitorConfig);
            livePipelineOptions.eventQueueCapacity = appConfig.events.queueCapacity;
            livePipelineOptions.eventCallback = [&dispatcher](const asset::AssetEvent& event) {
                dispatcher->dispatch(event);
            };
            livePipelineOptions.eventFlushCallback = [&dispatcher]() {
                dispatcher->flush();
            };

            const auto liveResult = live::runLiveCapturePipeline(
                *backendResult.backend,
                std::move(captureConfig),
                std::move(backendResult.initialStats),
                std::move(livePipelineOptions)
            );

            if (liveResult.error.has_value()) {
                throw std::runtime_error(*liveResult.error);
            }

            // Write final assets snapshot
            storage::SQLiteWriter writer(*appConfig.database.sqlitePath);
            writer.writeAssets(liveResult.assets);

        } else {
            capture::PacketCaptureBackend backend;
            const auto pcapResult = backend.readPcapFile(
                *appConfig.capture.pcapPath,
                appConfig.capture.packetFilter);
            if (pcapResult.error.has_value()) {
                throw std::runtime_error(*pcapResult.error);
            }

            monitor::AssetMonitorConfig monitorConfig;
            monitorConfig.detector.interfaceName = "pcap";
            monitorConfig.detector.localNetworks = appConfig.network.localNetworks;
            monitorConfig.detector.ignoredNetworks = appConfig.network.ignoredNetworks;
            monitorConfig.detector.flipFlopWindowSeconds = appConfig.events.flipFlopWindowSeconds;
            monitorConfig.detector.reappearanceThresholdSeconds = appConfig.events.reappearanceThresholdSeconds;
            monitorConfig.eventRateLimitSeconds = appConfig.events.rateLimitSeconds;

            monitor::AssetMonitor monitor(
                std::move(monitorConfig),
                [&dispatcher](const asset::AssetEvent& event) {
                    dispatcher->dispatch(event);
                }
            );

            for (const auto& packet : pcapResult.packets) {
                if (packet.linkType != capture::LinkType::Ethernet) {
                    continue;
                }
                const auto observations = parser::parseEthernetObservations(
                    packet.bytes,
                    toObservationTimestamp(packet.timestamp));
                for (const auto& obs : observations) {
                    monitor.applyObservation(obs);
                }
            }

            dispatcher->flush();

            // Save results to SQLite
            storage::SQLiteWriter writer(*appConfig.database.sqlitePath);
            writer.writeAssets(monitor.assets());
        }

        statusText_ = "Finished successfully";
        lastError_ = "";
        validationError_ = "";
        const auto sessionId = activeSessionId_.load();
        if (sessionId > 0) {
            storage::SQLiteWriter writer(*appConfig.database.sqlitePath);
            int assetCount = 0;
            int eventCount = 0;
            (void)writer.countAssets(assetCount);
            (void)writer.countEvents(eventCount);
            (void)writer.finishAnalysisSession(sessionId, "Completed", assetCount, eventCount);
        }
        emit lastErrorChanged();
        emit validationErrorChanged();
    }
    catch (const std::exception& e) {
        qWarning() << "Error in CaptureWorker thread:" << e.what();
        std::string errStr = e.what();
        std::string diagnosticStr;

        if (isLive_ && (errStr.find("Permission denied") != std::string::npos ||
                       errStr.find("permission") != std::string::npos ||
                       errStr.find("You don't have permission") != std::string::npos ||
                       errStr.find("CAP_NET_RAW") != std::string::npos ||
                       errStr.find("socket:") != std::string::npos ||
                       errStr.find("pcap_activate") != std::string::npos)) {
            if (geteuid() != 0) {
                std::string exePath = getExecutablePath();
                diagnosticStr = "Capture permission denied. Grant capabilities with:\n"
                                "sudo setcap cap_net_raw,cap_net_admin=eip " + exePath;
            }
        }

        // Keep lastError_ and statusText_ as single-line strings so the UI
        // banner and status bar never overflow with multiline diagnostic text.
        // The full diagnostic goes into recentFailureSummary_ which is shown
        // in a dedicated scrollable area (System Health / Capture view).
        const QString shortError = QString::fromStdString(errStr);
        statusText_ = "Capture error";
        lastError_ = shortError;
        validationError_ = shortError;

        const QString fullSummary = diagnosticStr.empty()
            ? shortError
            : shortError + "\n\n" + QString::fromStdString(diagnosticStr);
        recordRuntimeFailure(fullSummary);
        const auto sessionId = activeSessionId_.load();
        if (sessionId > 0) {
            try {
                storage::SQLiteWriter writer(sqlitePath_.toStdString());
                int assetCount = 0;
                int eventCount = 0;
                (void)writer.countAssets(assetCount);
                (void)writer.countEvents(eventCount);
                (void)writer.finishAnalysisSession(sessionId, "Failed", assetCount, eventCount, errStr);
            } catch (const std::exception& sessionError) {
                qWarning() << "Failed to record session failure:" << sessionError.what();
            }
        }
        emit lastErrorChanged();
        emit validationErrorChanged();
    }

    activeSessionId_ = 0;
    isRunning_ = false;
    emit isRunningChanged();
    emit statusTextChanged();
    emit captureFinished();
}

void CaptureController::saveSettingsToDb()
{
    QSettings settings("PNAD", "PNAD Desktop");
    settings.setValue("interfaceName", interfaceName_);
    settings.setValue("pcapPath", pcapPath_);
    settings.setValue("configPath", configPath_);
    settings.setValue("profileName", profileName_);
    settings.setValue("packetFilter", packetFilter_);
    settings.setValue("captureBackend", captureBackend_);
    settings.setValue("outputFormat", outputFormat_);
    settings.setValue("isLive", isLive_);
    settings.setValue("eventRateLimitSeconds", eventRateLimitSeconds_);
    settings.setValue("eventQueueCapacity", eventQueueCapacity_);
    settings.setValue("flipFlopWindowSeconds", flipFlopWindowSeconds_);
    settings.setValue("reappearanceThresholdSeconds", reappearanceThresholdSeconds_);
    settings.setValue("localNetworks", localNetworks_);
    settings.setValue("ignoredNetworks", ignoredNetworks_);
    settings.setValue("sqlitePath", sqlitePath_);
    settings.sync();

    try {
        storage::SQLiteWriter writer(sqlitePath_.toStdString());

        writer.saveSetting("sqlitePath", sqlitePath_.toStdString());
        writer.saveSetting("interfaceName", interfaceName_.toStdString());
        writer.saveSetting("pcapPath", pcapPath_.toStdString());
        writer.saveSetting("configPath", configPath_.toStdString());
        writer.saveSetting("profileName", profileName_.toStdString());
        writer.saveSetting("packetFilter", packetFilter_.toStdString());
        writer.saveSetting("captureBackend", captureBackend_.toStdString());
        writer.saveSetting("outputFormat", outputFormat_.toStdString());
        writer.saveSetting("isLive", isLive_ ? "true" : "false");
        writer.saveSetting("eventRateLimitSeconds", std::to_string(eventRateLimitSeconds_));
        writer.saveSetting("eventQueueCapacity", std::to_string(eventQueueCapacity_));
        writer.saveSetting("flipFlopWindowSeconds", std::to_string(flipFlopWindowSeconds_));
        writer.saveSetting("reappearanceThresholdSeconds", std::to_string(reappearanceThresholdSeconds_));
        writer.saveSetting("localNetworks", localNetworks_.join(",").toStdString());
        writer.saveSetting("ignoredNetworks", ignoredNetworks_.join(",").toStdString());
    }
    catch (const std::exception& e) {
        qWarning() << "Failed to save settings to DB:" << e.what();
    }
}

void CaptureController::loadSettingsFromDb()
{
    QSettings settings("PNAD", "PNAD Desktop");
    if (settings.contains("sqlitePath")) {
        setSqlitePath(settings.value("sqlitePath").toString());
    }
    if (settings.contains("interfaceName")) {
        setInterfaceName(settings.value("interfaceName").toString());
    }
    if (settings.contains("pcapPath")) {
        setPcapPath(settings.value("pcapPath").toString());
    }
    if (settings.contains("configPath")) {
        setConfigPath(settings.value("configPath").toString());
    }
    if (settings.contains("profileName")) {
        setProfileName(settings.value("profileName").toString());
    }
    if (settings.contains("packetFilter")) {
        setPacketFilter(settings.value("packetFilter").toString());
    }
    if (settings.contains("captureBackend")) {
        setCaptureBackend(settings.value("captureBackend").toString());
    }
    if (settings.contains("outputFormat")) {
        setOutputFormat(settings.value("outputFormat").toString());
    }
    if (settings.contains("isLive")) {
        setIsLive(settings.value("isLive").toBool());
    }
    if (settings.contains("eventRateLimitSeconds")) {
        setEventRateLimitSeconds(settings.value("eventRateLimitSeconds").toInt());
    }
    if (settings.contains("eventQueueCapacity")) {
        setEventQueueCapacity(settings.value("eventQueueCapacity").toInt());
    }
    if (settings.contains("flipFlopWindowSeconds")) {
        setFlipFlopWindowSeconds(settings.value("flipFlopWindowSeconds").toInt());
    }
    if (settings.contains("reappearanceThresholdSeconds")) {
        setReappearanceThresholdSeconds(settings.value("reappearanceThresholdSeconds").toInt());
    }
    if (settings.contains("localNetworks")) {
        setLocalNetworks(settings.value("localNetworks").toStringList());
    }
    if (settings.contains("ignoredNetworks")) {
        setIgnoredNetworks(settings.value("ignoredNetworks").toStringList());
    }

    try {
        // If directory doesn't exist, we skip loading (uncreated db)
        std::string dbPath = sqlitePath_.toStdString();
        storage::SQLiteWriter writer(dbPath);

        std::string val;
        if (!writer.getSetting("sqlitePath", val).has_value()) {
            setSqlitePath(QString::fromStdString(val));
        }
        if (!writer.getSetting("interfaceName", val).has_value()) {
            setInterfaceName(QString::fromStdString(val));
        }
        if (!writer.getSetting("pcapPath", val).has_value()) {
            setPcapPath(QString::fromStdString(val));
        }
        if (!writer.getSetting("configPath", val).has_value()) {
            setConfigPath(QString::fromStdString(val));
        }
        if (!writer.getSetting("profileName", val).has_value()) {
            setProfileName(QString::fromStdString(val));
        }
        if (!writer.getSetting("packetFilter", val).has_value()) {
            setPacketFilter(QString::fromStdString(val));
        }
        if (!writer.getSetting("captureBackend", val).has_value()) {
            setCaptureBackend(QString::fromStdString(val));
        }
        if (!writer.getSetting("outputFormat", val).has_value()) {
            setOutputFormat(QString::fromStdString(val));
        }
        if (!writer.getSetting("isLive", val).has_value()) {
            setIsLive(val == "true");
        }
        if (!writer.getSetting("eventRateLimitSeconds", val).has_value()) {
            setEventRateLimitSeconds(std::stoi(val));
        }
        if (!writer.getSetting("eventQueueCapacity", val).has_value()) {
            setEventQueueCapacity(std::stoi(val));
        }
        if (!writer.getSetting("flipFlopWindowSeconds", val).has_value()) {
            setFlipFlopWindowSeconds(std::stoi(val));
        }
        if (!writer.getSetting("reappearanceThresholdSeconds", val).has_value()) {
            setReappearanceThresholdSeconds(std::stoi(val));
        }
        if (!writer.getSetting("localNetworks", val).has_value()) {
            QString str = QString::fromStdString(val);
            setLocalNetworks(str.isEmpty() ? QStringList() : str.split(","));
        }
        if (!writer.getSetting("ignoredNetworks", val).has_value()) {
            QString str = QString::fromStdString(val);
            setIgnoredNetworks(str.isEmpty() ? QStringList() : str.split(","));
        }
    }
    catch (const std::exception& e) {
        qWarning() << "Failed to load settings from DB:" << e.what();
    }
}

bool CaptureController::validateSettings()
{
    if (!validateStoragePath()) {
        return false;
    }
    const auto result = buildCurrentConfig();
    if (result.error.has_value()) {
        setValidationError(QString::fromStdString(*result.error));
        return false;
    }
    setValidationError("");
    return true;
}

bool CaptureController::validatePreferences()
{
    if (!validateStoragePath()) {
        return false;
    }

    config::RuntimeEnvironment runtimeEnvironment;
    runtimeEnvironment.databaseConfigured = false;
    runtimeEnvironment.eventNdjsonPath = "logs/events.ndjson";

    DesktopRunConfig config = currentDesktopRunConfig();
    config.mode = DesktopRunMode::LiveCapture;
    config.liveCapture.interfaceName = "preferences-validation";

    const auto result = buildDesktopAppConfig(config, runtimeEnvironment, {});
    if (result.error.has_value()) {
        setValidationError(QString::fromStdString(*result.error));
        return false;
    }

    setValidationError("");
    return true;
}

bool CaptureController::validateLiveCaptureRequest()
{
    const bool previousMode = isLive_;
    isLive_ = true;
    const bool valid = validateSettings();
    isLive_ = previousMode;
    emit isLiveChanged();
    return valid;
}

bool CaptureController::validatePcapAnalysisRequest()
{
    const bool previousMode = isLive_;
    isLive_ = false;
    const bool valid = validateSettings() && validatePcapSource();
    isLive_ = previousMode;
    emit isLiveChanged();
    return valid;
}

QString CaptureController::choosePcapFile()
{
    return QFileDialog::getOpenFileName(
        nullptr,
        "Select PCAP File for Analysis",
        pcapPath_.isEmpty() ? QDir::homePath() : pcapPath_,
        "PCAP Files (*.pcap *.pcapng);;All Files (*)");
}

QString CaptureController::chooseConfigFile()
{
    return QFileDialog::getOpenFileName(
        nullptr,
        "Select YAML Config File",
        configPath_.isEmpty() ? QDir::homePath() : configPath_,
        "YAML Files (*.yaml *.yml);;All Files (*)");
}

config::ConfigResult CaptureController::buildCurrentConfig() const
{
    config::RuntimeEnvironment runtimeEnvironment;
    runtimeEnvironment.databaseConfigured = false;
    runtimeEnvironment.eventNdjsonPath = "logs/events.ndjson";

    config::BuildConfigOptions buildOptions;
    return buildDesktopAppConfig(currentDesktopRunConfig(), runtimeEnvironment, buildOptions);
}

DesktopRunConfig CaptureController::currentDesktopRunConfig() const
{
    DesktopRunConfig config;
    config.mode = isLive_ ? DesktopRunMode::LiveCapture : DesktopRunMode::PcapAnalysis;
    config.liveCapture.interfaceName = interfaceName_.trimmed().toStdString();
    config.pcapAnalysis.pcapPath = pcapPath_.trimmed().toStdString();
    if (!configPath_.trimmed().isEmpty()) {
        config.engine.preferencesFile = configPath_.trimmed().toStdString();
    }
    if (!profileName_.trimmed().isEmpty()) {
        config.engine.presetName = profileName_.trimmed().toStdString();
    }
    config.engine.captureFilter = packetFilter_.trimmed().toStdString();
    config.engine.backendPolicy = captureBackend_.trimmed().toStdString();
    config.engine.localDatabasePath = sqlitePath_.trimmed().toStdString();
    config.engine.duplicateEventSuppressionSeconds = eventRateLimitSeconds_;
    config.engine.eventBufferCapacity = eventQueueCapacity_;
    config.engine.ipChangeDetectionWindowSeconds = flipFlopWindowSeconds_;
    config.engine.reappearanceDetectionThresholdSeconds = reappearanceThresholdSeconds_;

    for (const auto& net : localNetworks_) {
        const auto trimmed = net.trimmed();
        if (!trimmed.isEmpty()) {
            config.engine.localNetworkCidrs.push_back(trimmed.toStdString());
        }
    }
    for (const auto& net : ignoredNetworks_) {
        const auto trimmed = net.trimmed();
        if (!trimmed.isEmpty()) {
            config.engine.ignoredNetworkCidrs.push_back(trimmed.toStdString());
        }
    }

    config.exportPreferences.format = outputFormat_.trimmed().toStdString();
    return config;
}

bool CaptureController::validateStoragePath()
{
    const QString path = sqlitePath_.trimmed();
    if (path.isEmpty()) {
        setValidationError("Choose a local database location.");
        return false;
    }

    const QFileInfo databaseInfo(path);
    const QDir parentDir(databaseInfo.absoluteDir());
    if (!parentDir.exists()) {
        if (!QDir().mkpath(parentDir.absolutePath())) {
            setValidationError("Choose a local database folder that can be created.");
            return false;
        }
    }

    const QFileInfo parentInfo(parentDir.absolutePath());
    if (!parentDir.exists() || !parentDir.isReadable() || !parentInfo.isWritable()) {
        setValidationError("Choose a writable local database folder.");
        return false;
    }
    if (databaseInfo.exists() && !databaseInfo.isWritable()) {
        setValidationError("Choose a writable local database file.");
        return false;
    }

    setValidationError("");
    return true;
}

bool CaptureController::validatePcapSource()
{
    const QFileInfo fileInfo(pcapPath_.trimmed());
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        setValidationError("Choose a readable PCAP or PCAPNG file before starting analysis.");
        return false;
    }
    const QString suffix = fileInfo.suffix().toLower();
    if (suffix != "pcap" && suffix != "pcapng") {
        setValidationError("Choose a supported PCAP or PCAPNG file.");
        return false;
    }
    setValidationError("");
    return true;
}

void CaptureController::recordRuntimeFailure(const QString& summary)
{
    recentFailureSummary_ = summary;
    emit recentFailureSummaryChanged();

    const QFileInfo logInfo(runtimeLogPath_);
    QDir().mkpath(logInfo.absoluteDir().absolutePath());
    QFile file(runtimeLogPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        return;
    }

    QTextStream stream(&file);
    stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
           << " " << summary << "\n";
}

void CaptureController::setValidationError(const QString& error)
{
    if (validationError_ == error) {
        return;
    }
    validationError_ = error;
    lastError_ = error;
    emit validationErrorChanged();
    emit lastErrorChanged();
}

} // namespace asset_discovery::gui
