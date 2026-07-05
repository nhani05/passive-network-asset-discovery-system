#pragma once

#include "pnad/config/AppConfig.hpp"
#include "pnad/gui/DesktopRunConfig.hpp"

#include <QObject>
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
    Q_PROPERTY(int eventRateLimitSeconds READ eventRateLimitSeconds WRITE setEventRateLimitSeconds NOTIFY eventRateLimitSecondsChanged)
    Q_PROPERTY(int eventQueueCapacity READ eventQueueCapacity WRITE setEventQueueCapacity NOTIFY eventQueueCapacityChanged)
    Q_PROPERTY(int flipFlopWindowSeconds READ flipFlopWindowSeconds WRITE setFlipFlopWindowSeconds NOTIFY flipFlopWindowSecondsChanged)
    Q_PROPERTY(int reappearanceThresholdSeconds READ reappearanceThresholdSeconds WRITE setReappearanceThresholdSeconds NOTIFY reappearanceThresholdSecondsChanged)
    Q_PROPERTY(QStringList localNetworks READ localNetworks WRITE setLocalNetworks NOTIFY localNetworksChanged)
    Q_PROPERTY(QStringList ignoredNetworks READ ignoredNetworks WRITE setIgnoredNetworks NOTIFY ignoredNetworksChanged)
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
    void setPacketFilter(const QString& val) { if (packetFilter_ != val) { packetFilter_ = val; emit packetFilterChanged(); } }

    QString captureBackend() const { return captureBackend_; }
    void setCaptureBackend(const QString& val) { if (captureBackend_ != val) { captureBackend_ = val; emit captureBackendChanged(); } }

    QString outputFormat() const { return outputFormat_; }
    void setOutputFormat(const QString& val) { if (outputFormat_ != val) { outputFormat_ = val; emit outputFormatChanged(); } }

    bool isLive() const { return isLive_; }
    void setIsLive(bool val) { if (isLive_ != val) { isLive_ = val; emit isLiveChanged(); } }

    int eventRateLimitSeconds() const { return eventRateLimitSeconds_; }
    void setEventRateLimitSeconds(int val) { if (eventRateLimitSeconds_ != val) { eventRateLimitSeconds_ = val; emit eventRateLimitSecondsChanged(); } }

    int eventQueueCapacity() const { return eventQueueCapacity_; }
    void setEventQueueCapacity(int val) { if (eventQueueCapacity_ != val) { eventQueueCapacity_ = val; emit eventQueueCapacityChanged(); } }

    int flipFlopWindowSeconds() const { return flipFlopWindowSeconds_; }
    void setFlipFlopWindowSeconds(int val) { if (flipFlopWindowSeconds_ != val) { flipFlopWindowSeconds_ = val; emit flipFlopWindowSecondsChanged(); } }

    int reappearanceThresholdSeconds() const { return reappearanceThresholdSeconds_; }
    void setReappearanceThresholdSeconds(int val) { if (reappearanceThresholdSeconds_ != val) { reappearanceThresholdSeconds_ = val; emit reappearanceThresholdSecondsChanged(); } }

    QStringList localNetworks() const { return localNetworks_; }
    void setLocalNetworks(const QStringList& val) { if (localNetworks_ != val) { localNetworks_ = val; emit localNetworksChanged(); } }

    QStringList ignoredNetworks() const { return ignoredNetworks_; }
    void setIgnoredNetworks(const QStringList& val) { if (ignoredNetworks_ != val) { ignoredNetworks_ = val; emit ignoredNetworksChanged(); } }

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
    void eventRateLimitSecondsChanged();
    void eventQueueCapacityChanged();
    void flipFlopWindowSecondsChanged();
    void reappearanceThresholdSecondsChanged();
    void localNetworksChanged();
    void ignoredNetworksChanged();
    void sqlitePathChanged();
    void isRunningChanged();
    void statusTextChanged();
    void lastErrorChanged();
    void validationErrorChanged();
    void recentFailureSummaryChanged();
    void runtimeLogPathChanged();
    void captureFinished();

private:
    void runCaptureWorker();
    config::ConfigResult buildCurrentConfig() const;
    DesktopRunConfig currentDesktopRunConfig() const;
    bool validateStoragePath();
    bool validatePcapSource();
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
    QString packetFilter_ = "arp or udp port 67 or udp port 68";
    QString captureBackend_ = "auto";
    QString outputFormat_ = "json";
    bool isLive_ = true;
    int eventRateLimitSeconds_ = 60;
    int eventQueueCapacity_ = 1024;
    int flipFlopWindowSeconds_ = 300;
    int reappearanceThresholdSeconds_ = 15552000;
    QStringList localNetworks_;
    QStringList ignoredNetworks_;
    QString sqlitePath_ = "pnad.db";
    QString statusText_ = "Stopped";
    QString lastError_;
    QString validationError_;
    QString recentFailureSummary_;
    QString runtimeLogPath_ = "logs/pnad-runtime.log";
};

} // namespace asset_discovery::gui
