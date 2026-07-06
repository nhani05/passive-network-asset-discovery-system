#pragma once

#include "pnad/event/AssetEvent.hpp"

#include <QString>
#include <QStringList>

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>

namespace asset_discovery::gui {

enum class EmailTlsMode {
    None,
    StartTls,
    Tls,
};

struct EmailAlertSettings {
    bool enabled = false;
    QString smtpHost;
    int smtpPort = 587;
    EmailTlsMode tlsMode = EmailTlsMode::StartTls;
    QString username;
    QString passwordEnvVar = "PNAD_SMTP_PASSWORD";
    QString passwordValue;
    QString senderAddress;
    QStringList recipients;
};

struct MailMessage {
    QString from;
    QStringList to;
    QString subject;
    QString body;
};

class MailSender {
public:
    virtual ~MailSender() = default;
    virtual std::optional<QString> send(const MailMessage& message, const EmailAlertSettings& settings) = 0;
};

class CurlSmtpMailSender final : public MailSender {
public:
    std::optional<QString> send(const MailMessage& message, const EmailAlertSettings& settings) override;
};

class EmailAlertNotifier final {
public:
    using FailureCallback = std::function<void(const QString&)>;

    explicit EmailAlertNotifier(
        EmailAlertSettings settings = {},
        std::unique_ptr<MailSender> sender = std::make_unique<CurlSmtpMailSender>());
    ~EmailAlertNotifier();

    EmailAlertNotifier(const EmailAlertNotifier&) = delete;
    EmailAlertNotifier& operator=(const EmailAlertNotifier&) = delete;

    std::optional<QString> validate() const;
    bool handleAssetEvent(const asset::AssetEvent& event);
    void resetSession();
    void stop();
    void setFailureCallback(FailureCallback callback);

    static MailMessage formatNewAssetMessage(const asset::AssetEvent& event, const EmailAlertSettings& settings);
    static std::optional<QString> validateSettings(const EmailAlertSettings& settings);

private:
    void workerLoop();
    void reportFailure(const QString& message);

    EmailAlertSettings settings_;
    std::unique_ptr<MailSender> sender_;
    std::set<std::string> alertedMacs_;
    std::deque<MailMessage> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
    std::thread worker_;
    FailureCallback failureCallback_;
};

QString emailTlsModeName(EmailTlsMode mode);
EmailTlsMode parseEmailTlsMode(const QString& value);
QStringList splitEmailRecipients(const QString& value);
QString joinEmailRecipients(const QStringList& recipients);

} // namespace asset_discovery::gui
