#include "pnad/gui/EmailAlertNotifier.hpp"

#include <QByteArray>
#include <QFile>
#include <QProcess>
#include <QTemporaryFile>
#include <QTextStream>

#include <cstdlib>
#include <utility>

namespace asset_discovery::gui {
namespace {

constexpr std::size_t kMaxQueuedEmails = 64;

QString trimValue(const QString& value)
{
    return value.trimmed();
}

QString smtpUrl(const EmailAlertSettings& settings)
{
    const QString scheme = settings.tlsMode == EmailTlsMode::Tls ? "smtps" : "smtp";
    return scheme + "://" + settings.smtpHost.trimmed() + ":" + QString::number(settings.smtpPort);
}

QString passwordFromEnvironment(const QString& envVar)
{
    const QByteArray name = envVar.trimmed().toLocal8Bit();
    if (name.isEmpty()) {
        return {};
    }
    const char* value = std::getenv(name.constData());
    return value == nullptr ? QString() : QString::fromLocal8Bit(value);
}

QString messageData(const MailMessage& message)
{
    QString output;
    QTextStream stream(&output);
    stream << "From: " << message.from << "\r\n";
    stream << "To: " << message.to.join(", ") << "\r\n";
    stream << "Subject: " << message.subject << "\r\n";
    stream << "Content-Type: text/plain; charset=utf-8\r\n";
    stream << "\r\n";
    stream << message.body << "\r\n";
    return output;
}

QString optionalValue(const std::optional<std::string>& value)
{
    return value.has_value() && !value->empty() ? QString::fromStdString(*value) : "-";
}

} // namespace

QString emailTlsModeName(EmailTlsMode mode)
{
    switch (mode) {
    case EmailTlsMode::None:
        return "none";
    case EmailTlsMode::StartTls:
        return "starttls";
    case EmailTlsMode::Tls:
        return "tls";
    }
    return "starttls";
}

EmailTlsMode parseEmailTlsMode(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == "none") {
        return EmailTlsMode::None;
    }
    if (normalized == "tls" || normalized == "ssl" || normalized == "smtps") {
        return EmailTlsMode::Tls;
    }
    return EmailTlsMode::StartTls;
}

QStringList splitEmailRecipients(const QString& value)
{
    QStringList output;
    for (const auto& item : value.split(',', Qt::SkipEmptyParts)) {
        const QString trimmed = item.trimmed();
        if (!trimmed.isEmpty()) {
            output.push_back(trimmed);
        }
    }
    return output;
}

QString joinEmailRecipients(const QStringList& recipients)
{
    QStringList cleaned;
    for (const auto& recipient : recipients) {
        const QString trimmed = recipient.trimmed();
        if (!trimmed.isEmpty()) {
            cleaned.push_back(trimmed);
        }
    }
    return cleaned.join(", ");
}

std::optional<QString> CurlSmtpMailSender::send(const MailMessage& message, const EmailAlertSettings& settings)
{
    QTemporaryFile payload;
    if (!payload.open()) {
        return "could not create temporary email payload";
    }

    const QString data = messageData(message);
    payload.write(data.toUtf8());
    payload.flush();

    QStringList args;
    args << "--silent" << "--show-error"
         << "--url" << smtpUrl(settings)
         << "--mail-from" << message.from;
    for (const auto& recipient : message.to) {
        args << "--mail-rcpt" << recipient;
    }
    if (settings.tlsMode == EmailTlsMode::StartTls) {
        args << "--ssl-reqd";
    }
    const QString username = settings.username.trimmed();
    if (!username.isEmpty()) {
        const QString password = settings.passwordValue.trimmed().isEmpty()
            ? passwordFromEnvironment(settings.passwordEnvVar)
            : settings.passwordValue;
        if (password.isEmpty()) {
            return "SMTP password is not configured. Set PNAD_EMAIL_PASSWORD or " + settings.passwordEnvVar;
        }
        args << "--user" << (username + ":" + password);
    }
    args << "--upload-file" << payload.fileName();

    QProcess process;
    process.start("curl", args);
    if (!process.waitForStarted(5000)) {
        return "could not start curl for SMTP delivery";
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        return "SMTP delivery timed out";
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return stderrText.isEmpty() ? "SMTP delivery failed" : stderrText;
    }
    return std::nullopt;
}

EmailAlertNotifier::EmailAlertNotifier(EmailAlertSettings settings, std::unique_ptr<MailSender> sender)
    : settings_(std::move(settings))
    , sender_(std::move(sender))
    , worker_(&EmailAlertNotifier::workerLoop, this)
{
}

EmailAlertNotifier::~EmailAlertNotifier()
{
    stop();
}

std::optional<QString> EmailAlertNotifier::validate() const
{
    return validateSettings(settings_);
}

std::optional<QString> EmailAlertNotifier::validateSettings(const EmailAlertSettings& settings)
{
    if (!settings.enabled) {
        return std::nullopt;
    }
    if (trimValue(settings.smtpHost).isEmpty()) {
        return "Email alerts require an SMTP host.";
    }
    if (settings.smtpPort <= 0 || settings.smtpPort > 65535) {
        return "Email alerts require a valid SMTP port.";
    }
    if (trimValue(settings.senderAddress).isEmpty()) {
        return "Email alerts require a sender address.";
    }
    if (splitEmailRecipients(joinEmailRecipients(settings.recipients)).isEmpty()) {
        return "Email alerts require at least one recipient.";
    }
    if (!trimValue(settings.username).isEmpty()
        && trimValue(settings.passwordValue).isEmpty()) {
        const QString passwordHint = trimValue(settings.passwordEnvVar).isEmpty()
            ? QString("PNAD_EMAIL_PASSWORD")
            : QString("PNAD_EMAIL_PASSWORD or ") + trimValue(settings.passwordEnvVar);
        return "Email alerts require " + passwordHint + " when SMTP username is set.";
    }
    return std::nullopt;
}

MailMessage EmailAlertNotifier::formatNewAssetMessage(const asset::AssetEvent& event, const EmailAlertSettings& settings)
{
    const QString mac = optionalValue(event.macAddress);
    MailMessage message;
    message.from = settings.senderAddress.trimmed();
    message.to = settings.recipients;
    message.subject = "[PNAD] New asset discovered: " + mac;

    QString body;
    QTextStream stream(&body);
    stream << "A new asset was discovered.\n\n";
    stream << "Time: " << QString::fromStdString(asset::formatEventTimestamp(event.timestamp)) << "\n";
    stream << "MAC: " << mac << "\n";
    stream << "IP: " << optionalValue(event.ipAddress) << "\n";
    stream << "Hostname: " << optionalValue(event.hostname) << "\n";
    stream << "Protocol: " << (event.protocol.empty() ? "-" : QString::fromStdString(event.protocol)) << "\n";
    stream << "Interface: " << (event.interfaceName.empty() ? "-" : QString::fromStdString(event.interfaceName)) << "\n";
    message.body = body;
    return message;
}

bool EmailAlertNotifier::handleAssetEvent(const asset::AssetEvent& event)
{
    if (!settings_.enabled || event.type != asset::AssetEventType::NewAsset || !event.macAddress.has_value()) {
        return false;
    }

    const std::string mac = *event.macAddress;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!alertedMacs_.insert(mac).second) {
        return false;
    }
    if (queue_.size() >= kMaxQueuedEmails) {
        reportFailure("Email alert queue is full; dropping alert for " + QString::fromStdString(mac));
        return false;
    }
    queue_.push_back(formatNewAssetMessage(event, settings_));
    cv_.notify_one();
    return true;
}

void EmailAlertNotifier::resetSession()
{
    std::lock_guard<std::mutex> lock(mutex_);
    alertedMacs_.clear();
    queue_.clear();
}

void EmailAlertNotifier::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void EmailAlertNotifier::setFailureCallback(FailureCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    failureCallback_ = std::move(callback);
}

void EmailAlertNotifier::workerLoop()
{
    while (true) {
        MailMessage message;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return stopping_ || !queue_.empty();
            });
            if (stopping_ && queue_.empty()) {
                return;
            }
            message = std::move(queue_.front());
            queue_.pop_front();
        }

        if (sender_) {
            if (const auto error = sender_->send(message, settings_); error.has_value()) {
                reportFailure("Email alert failed: " + *error);
            }
        }
    }
}

void EmailAlertNotifier::reportFailure(const QString& message)
{
    FailureCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = failureCallback_;
    }
    if (callback) {
        callback(message);
    }
}

} // namespace asset_discovery::gui
