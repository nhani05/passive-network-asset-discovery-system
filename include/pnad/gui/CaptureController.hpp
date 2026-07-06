#pragma once

#include "pnad/config/AppConfig.hpp"
#include "pnad/constants/BackendConstants.hpp"
#include "pnad/constants/CaptureConstants.hpp"
#include "pnad/constants/CliConstants.hpp"
#include "pnad/constants/ConfigConstants.hpp"
#include "pnad/gui/DesktopRunConfig.hpp"

#include <QObject>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace asset_discovery::gui {

class CaptureController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString interfaceName READ interfaceName WRITE setInterfaceName NOTIFY interfaceNameChanged)
    Q_PROPERTY(QString pcapPath READ pcapPath WRITE setPcapPath NOTIFY pcapPathChanged)
    Q_PROPERTY(QString configPath READ configPath WRITE setConfigPath NOTIFY configPathChanged)
    Q_PROPERTY(QString profileName READ profileName WRITE setProfileName NOTIFY profileNameChanged)
    Q_PROPERTY(QString packetFilter READ packetFilter WRITE setPacketFilter NOTIFY packetFilterChanged)
    Q_PROPERTY(QString captureBackend READ captureBackend WRITE setCaptureBackend NOTIFY captureBackendChanged)
    Q_PROPERTY(QString outputFormat READ outputFormat WRITE setOutputFormat NOTIFY outputFormatChanged)
    Q_PROPERTY(bool isLive READ isLive WRITE setIsLive NOTIFY isLiveChanged)
    Q_PROPERTY(QString sqlitePath READ sqlitePath WRITE setSqlitePath NOTIFY sqlitePathChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString validationError READ validationError NOTIFY validationErrorChanged)
    Q_PROPERTY(QString recentFailureSummary READ recentFailureSummary NOTIFY recentFailureSummaryChanged)
    Q_PROPERTY(QString runtimeLogPath READ runtimeLogPath NOTIFY runtimeLogPathChanged)

public:
    explicit CaptureController(QObject* parent = nullptr);
    ~CaptureController() override;

    // Run controls
    Q_INVOKABLE void startCapture();
    Q_INVOKABLE void startLiveCapture();
    Q_INVOKABLE void startPcapAnalysis();
    Q_INVOKABLE void stopCapture();
    Q_INVOKABLE void loadDefaults();
    Q_INVOKABLE void clearError() { lastError_ = ""; emit lastErrorChanged(); }
    Q_INVOKABLE void saveSettingsToDb();
    Q_INVOKABLE void loadSettingsFromDb();
    Q_INVOKABLE bool validateSettings();
    Q_INVOKABLE bool validatePreferences();
    Q_INVOKABLE bool validateLiveCaptureRequest();
    Q_INVOKABLE bool validatePcapAnalysisRequest();
    Q_INVOKABLE QString choosePcapFile();
    Q_INVOKABLE QString chooseExportFile(const QString& format);
    Q_INVOKABLE QString chooseConfigFile();

    // Getters & Setters
    QString lastError() const { return lastError_; }
    QString validationError() const { return validationError_; }
    QString recentFailureSummary() const { return recentFailureSummary_; }
    QString runtimeLogPath() const { return runtimeLogPath_; }

    // Getters & Setters
    QString interfaceName() const { return interfaceName_; }
    void setInterfaceName(const QString& val) { if (interfaceName_ != val) { interfaceName_ = val; emit interfaceNameChanged(); } }

    QString pcapPath() const { return pcapPath_; }
    void setPcapPath(const QString& val) { if (pcapPath_ != val) { pcapPath_ = val; emit pcapPathChanged(); } }

    QString configPath() const { return configPath_; }
    void setConfigPath(const QString& val) { if (configPath_ != val) { configPath_ = val; emit configPathChanged(); } }

    QString profileName() const { return profileName_; }
    void setProfileName(const QString& val) { if (profileName_ != val) { profileName_ = val; emit profileNameChanged(); } }

    QString packetFilter() const { return packetFilter_; }
    void setPacketFilter(const QString& val);

    QString captureBackend() const { return captureBackend_; }
    void setCaptureBackend(const QString& val);

    QString outputFormat() const { return outputFormat_; }
    void setOutputFormat(const QString& val) { if (outputFormat_ != val) { outputFormat_ = val; emit outputFormatChanged(); } }

    bool isLive() const { return isLive_; }
    void setIsLive(bool val) { if (isLive_ != val) { isLive_ = val; emit isLiveChanged(); } }

    QString sqlitePath() const { return sqlitePath_; }
    void setSqlitePath(const QString& val) { if (sqlitePath_ != val) { sqlitePath_ = val; emit sqlitePathChanged(); } }

    bool isRunning() const { return isRunning_; }
    QString statusText() const { return statusText_; }

signals:
    void interfaceNameChanged();
    void pcapPathChanged();
    void configPathChanged();
    void profileNameChanged();
    void packetFilterChanged();
    void captureBackendChanged();
    void outputFormatChanged();
    void isLiveChanged();
    void sqlitePathChanged();
    void isRunningChanged();
    void statusTextChanged();
    void lastErrorChanged();
    void validationErrorChanged();
    void recentFailureSummaryChanged();
    void runtimeLogPathChanged();
    void eventLogMessage(QString timestamp, QString severity, QString source, QString message);
    void assetDiscovered(QVariantMap asset, bool isNew);
    void captureFinished();

private:
    void runCaptureWorker();
    config::ConfigResult buildCurrentConfig() const;
    DesktopRunConfig currentDesktopRunConfig() const;
    bool validateStoragePath();
    bool validatePcapSource();
    bool validateLiveCapturePermission();
    void recordRuntimeFailure(const QString& summary);
    void setValidationError(const QString& error);

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<std::int64_t> activeSessionId_{0};
    std::thread workerThread_;
    std::mutex workerMutex_;

    QString interfaceName_;
    QString pcapPath_;
    QString configPath_;
    QString profileName_;
    QString packetFilter_ = QString::fromLatin1(constants::capture::DefaultPacketFilter);
    QString captureBackend_ = QString::fromLatin1(constants::capture::BackendAutoName);
    QString outputFormat_ = QString::fromLatin1(constants::cli::OutputJson);
    bool isLive_ = false;
    QString sqlitePath_ = QString::fromLatin1(constants::config::DefaultSqlitePath);
    QString statusText_ = "Stopped";
    QString lastError_;
    QString validationError_;
    QString recentFailureSummary_;
    QString runtimeLogPath_ = QString::fromLatin1(constants::backend::DefaultRuntimeLogPath);
};

} // namespace asset_discovery::gui
