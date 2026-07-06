#include "pnad/gui/CaptureController.hpp"
#include "pnad/app/LiveCapturePipeline.hpp"
#include "pnad/capture/NetworkInterface.hpp"
#include "pnad/core/CoreSession.hpp"
#include "pnad/discovery/AssetMonitor.hpp"
#include "pnad/capture/PacketCapture.hpp"
#include "pnad/storage/SQLiteWriter.hpp"
#include "pnad/gui/DesktopRunConfig.hpp"
#include "pnad/constants/BackendConstants.hpp"
#include "pnad/constants/CaptureConstants.hpp"
#include "pnad/constants/CliConstants.hpp"
#include "pnad/constants/ConfigConstants.hpp"
#include "pnad/constants/GuiConstants.hpp"

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
#include <QVariantMap>
#include <algorithm>
#include <chrono>
#include <memory>
#include <set>
#include <unistd.h>
#include <limits.h>

namespace asset_discovery::gui {
namespace {

constexpr std::chrono::milliseconds kCoreGuiUpdateInterval{500};

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
    return constants::gui::ApplicationName;
}

QString currentLogTimestamp()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QStringList toStringList(const std::set<std::string>& values)
{
    QStringList list;
    for (const auto& value : values) {
        list.append(QString::fromStdString(value));
    }
    return list;
}

QVariantMap assetToDto(const asset::Asset& asset)
{
    QVariantMap dto;
    dto.insert("macAddress", QString::fromStdString(asset.macAddress));
    dto.insert("ipAddresses", toStringList(asset.ipAddresses));
    dto.insert("hostname", asset.hostname.has_value() ? QString::fromStdString(*asset.hostname) : QString());
    dto.insert("firstSeen", QString::fromStdString(asset::formatTimestamp(asset.firstSeen)));
    dto.insert("lastSeen", QString::fromStdString(asset::formatTimestamp(asset.lastSeen)));
    dto.insert("discoverySources", toStringList(asset.sources));
    dto.insert("vendor", "Unknown");
    dto.insert("deviceType", "Unknown");
    dto.insert("os", "Unknown");
    dto.insert("rawObservedMetadata", QString::fromStdString(mapToJson(asset.metadata)));
    dto.insert("risk", "Normal");
    return dto;
}

QString assetDisplayName(const asset::Asset& asset)
{
    if (asset.hostname.has_value() && !asset.hostname->empty()) {
        return QString::fromStdString(*asset.hostname);
    }
    if (!asset.ipAddresses.empty()) {
        return QString::fromStdString(*asset.ipAddresses.begin());
    }
    return QString::fromStdString(asset.macAddress);
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

void CaptureController::setCaptureBackend(const QString& val)
{
    const QString normalized = val.trimmed();
    const QString autoBackend = QString::fromLatin1(constants::capture::BackendAutoName);
    const QString pcapBackend = QString::fromLatin1(constants::capture::BackendPcapName);
    const QString backend = (normalized == autoBackend || normalized == pcapBackend)
        ? normalized
        : autoBackend;
    if (captureBackend_ != backend) {
        captureBackend_ = backend;
        emit captureBackendChanged();
    }
}

void CaptureController::setPacketFilter(const QString&)
{
    const QString fixedFilter = QString::fromLatin1(constants::capture::DefaultPacketFilter);
    if (packetFilter_ != fixedFilter) {
        packetFilter_ = fixedFilter;
        emit packetFilterChanged();
    }
}

void CaptureController::loadDefaults()
{
    interfaceName_ = "";
    pcapPath_ = "";
    configPath_ = "";
    profileName_ = "";
    packetFilter_ = QString::fromLatin1(constants::capture::DefaultPacketFilter);
    captureBackend_ = QString::fromLatin1(constants::capture::BackendAutoName);
    outputFormat_ = QString::fromLatin1(constants::cli::OutputJson);
    isLive_ = false;

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = ".";
    } else {
        QDir().mkpath(dataDir);
    }
    sqlitePath_ = dataDir + "/" + QString::fromLatin1(constants::config::DefaultSqlitePath);

    statusText_ = "Stopped";
    lastError_ = "";
    validationError_ = "";
    recentFailureSummary_ = "";
    runtimeLogPath_ = QString::fromLatin1(constants::backend::DefaultRuntimeLogPath);

    emit interfaceNameChanged();
    emit pcapPathChanged();
    emit configPathChanged();
    emit profileNameChanged();
    emit packetFilterChanged();
    emit captureBackendChanged();
    emit outputFormatChanged();
    emit isLiveChanged();
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

    setPacketFilter(QString());
    if (!validateSettings() || (!isLive_ && !validatePcapSource())) {
        statusText_ = "Configuration error";
        emit statusTextChanged();
        return;
    }
    if (isLive_ && !validateLiveCapturePermission()) {
        statusText_ = "Permission required";
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
            livePipelineOptions.coreParsersOnly = true;
            livePipelineOptions.parserWorkerCount = 1;
            monitor::AssetMonitorConfig monitorConfig;
            monitorConfig.interfaceName = appConfig.capture.interfaceName.value_or("");

            livePipelineOptions.monitorConfig = std::move(monitorConfig);
            auto lastAssetUiEmit = std::chrono::steady_clock::time_point{};
            livePipelineOptions.assetCallback = [this, lastAssetUiEmit](const asset::Asset& asset, bool isNew) mutable {
                const auto now = std::chrono::steady_clock::now();
                if (isNew || lastAssetUiEmit.time_since_epoch().count() == 0
                    || now - lastAssetUiEmit >= kCoreGuiUpdateInterval) {
                    emit assetDiscovered(assetToDto(asset), isNew);
                    lastAssetUiEmit = now;
                }
                if (isNew) {
                    emit eventLogMessage(
                        currentLogTimestamp(),
                        "info",
                        "asset.created",
                        "New asset discovered: " + assetDisplayName(asset));
                }
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
            core::CoreSessionOptions sessionOptions;
            sessionOptions.packetFilter = appConfig.capture.packetFilter;
            sessionOptions.pipelineOptions.coreParsersOnly = true;
            sessionOptions.pipelineOptions.parserWorkerCount = 1;
            sessionOptions.monitorConfig.interfaceName = constants::capture::PcapInterfaceName;

            auto pcapLoggedAssets = std::make_shared<std::set<std::string>>();
            core::CoreSession session({
                {},
                [this, pcapLoggedAssets](const asset::Asset& asset) {
                    emit assetDiscovered(assetToDto(asset), true);
                    if (pcapLoggedAssets->insert(asset.macAddress).second) {
                        emit eventLogMessage(
                            currentLogTimestamp(),
                            "info",
                            "asset.created",
                            "New asset discovered: " + assetDisplayName(asset));
                    }
                },
                {},
                {}
            });

            const auto sessionResult = session.analyzePcapFile(
                *appConfig.capture.pcapPath,
                std::move(sessionOptions));
            if (sessionResult.error.has_value()) {
                throw std::runtime_error(*sessionResult.error);
            }

            // Save results to SQLite
            storage::SQLiteWriter writer(*appConfig.database.sqlitePath);
            writer.writeAssets(sessionResult.assets);
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
        // in recentFailureSummary_, which the core view can surface without
        // overflowing the status banner.
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
        setPacketFilter(QString());
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

    DesktopRunConfig config = currentDesktopRunConfig();
    if (config.pcapAnalysis.pcapPath.empty()) {
        config.pcapAnalysis.pcapPath = "preferences-validation.pcap";
    }

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

QString CaptureController::chooseExportFile(const QString& format)
{
    const QString normalized = format.trimmed().toLower();
    const bool csv = normalized == QString::fromLatin1(constants::cli::OutputCsv);
    const QString extension = csv ? ".csv" : ".json";
    const QString filter = csv ? "CSV Files (*.csv);;All Files (*)" : "JSON Files (*.json);;All Files (*)";
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }

    QString path = QFileDialog::getSaveFileName(
        nullptr,
        "Choose Export Location",
        QDir(directory).filePath("assets" + extension),
        filter);
    if (path.isEmpty()) {
        return {};
    }

    const QFileInfo info(path);
    if (info.suffix().isEmpty()) {
        path += extension;
    }
    return path;
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

    config::BuildConfigOptions buildOptions;
    return buildDesktopAppConfig(currentDesktopRunConfig(), runtimeEnvironment, buildOptions);
}

DesktopRunConfig CaptureController::currentDesktopRunConfig() const
{
    DesktopRunConfig config;
    config.mode = DesktopRunMode::PcapAnalysis;
    config.pcapAnalysis.pcapPath = pcapPath_.trimmed().toStdString();
    config.engine.captureFilter = constants::capture::DefaultPacketFilter;
    config.engine.localDatabasePath = sqlitePath_.trimmed().toStdString();

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
    if (suffix != QString::fromLatin1(constants::capture::PcapFileExtension)
        && suffix != QString::fromLatin1(constants::capture::PcapNgFileExtension)) {
        setValidationError("Choose a supported PCAP or PCAPNG file.");
        return false;
    }
    setValidationError("");
    return true;
}

bool CaptureController::validateLiveCapturePermission()
{
    const QString selectedName = interfaceName_.trimmed();
    const auto interfaces = capture::listNetworkInterfaces();
    const auto selected = std::find_if(
        interfaces.begin(),
        interfaces.end(),
        [&](const auto& interfaceInfo) {
            return QString::fromStdString(interfaceInfo.systemName) == selectedName;
        });

    if (selected == interfaces.end()) {
        setValidationError("Choose an available network interface before starting Live Capture.");
        return false;
    }

    if (selected->captureAllowed) {
        setValidationError("");
        return true;
    }

    const QString diagnostic = selected->permissionDiagnostic.empty()
        ? QString("Live Capture requires packet capture permission before use.")
        : QString::fromStdString(selected->permissionDiagnostic);
    const QString fix = "Grant permission with:\nsudo setcap cap_net_raw,cap_net_admin=eip "
        + QString::fromStdString(getExecutablePath());

    setValidationError(diagnostic);
    recordRuntimeFailure(diagnostic + "\n\n" + fix);
    return false;
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
