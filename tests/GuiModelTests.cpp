#include "pnad/gui/AssetModel.hpp"
#include "pnad/gui/InterfaceModel.hpp"
#include "pnad/gui/LogModel.hpp"
#include "pnad/gui/EmailAlertNotifier.hpp"
#include "pnad/gui/CaptureController.hpp"
#include "pnad/storage/SQLiteWriter.hpp"

#include <QSettings>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void expectDebug(bool condition, const std::string& message, const std::string& detail)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << " (" << detail << ")\n";
        ++failures;
    }
}

bool fileContains(const std::string& path, const std::string& needle)
{
    std::ifstream file(path);
    const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return contents.find(needle) != std::string::npos;
}

class FakeMailSender final : public asset_discovery::gui::MailSender {
public:
    std::optional<QString> send(
        const asset_discovery::gui::MailMessage& message,
        const asset_discovery::gui::EmailAlertSettings&) override
    {
        messages.push_back(message);
        return failure;
    }

    std::vector<asset_discovery::gui::MailMessage> messages;
    std::optional<QString> failure;
};

asset_discovery::asset::AssetEvent sampleNewAssetEvent()
{
    asset_discovery::asset::AssetEvent event;
    event.timestamp = {1700000000, 123456};
    event.type = asset_discovery::asset::AssetEventType::NewAsset;
    event.severity = asset_discovery::asset::AssetEventSeverity::Info;
    event.macAddress = "aa:bb:cc:dd:ee:ff";
    event.ipAddress = "192.168.1.23";
    event.hostname = "laptop-user";
    event.protocol = "dhcp";
    event.interfaceName = "eth0";
    event.message = "New asset discovered";
    return event;
}

asset_discovery::gui::EmailAlertSettings validEmailSettings()
{
    asset_discovery::gui::EmailAlertSettings settings;
    settings.enabled = true;
    settings.smtpHost = "smtp.example.com";
    settings.smtpPort = 587;
    settings.tlsMode = asset_discovery::gui::EmailTlsMode::StartTls;
    settings.username = "pnad";
    settings.passwordEnvVar = "PNAD_SMTP_PASSWORD";
    settings.senderAddress = "pnad@example.com";
    settings.recipients = QStringList({"admin@example.com"});
    return settings;
}

void testCoreGuiModelsAndController()
{
    QSettings("PNAD", "PNAD Desktop").clear();
    asset_discovery::gui::CaptureController controller;
    expect(!controller.isLive(), "Default mode should be PCAP analysis");
    expect(controller.packetFilter().toStdString() == "arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353", "Default core BPF filter");
    expect(!controller.sqlitePath().isEmpty(), "Default database path should be resolved");
    expect(!controller.emailAlertsEnabled(), "Email alerts should be disabled by default");

    asset_discovery::gui::AssetModel assetModel;
    expect(assetModel.rowCount() == 0, "AssetModel should be initially empty");

    QVariantMap dtoAsset;
    dtoAsset.insert("macAddress", "aa:bb:cc:dd:ee:ff");
    dtoAsset.insert("ipAddresses", QStringList({"10.10.10.5"}));
    dtoAsset.insert("hostname", "dto-host");
    dtoAsset.insert("firstSeen", "1700000000");
    dtoAsset.insert("lastSeen", "1700000000");
    dtoAsset.insert("discoverySources", QStringList({"arp"}));
    assetModel.loadAssetDtos({dtoAsset});
    expect(assetModel.rowCount() == 1, "AssetModel should load direct DTO assets");
    expect(assetModel.data(assetModel.index(0, 0), asset_discovery::gui::AssetModel::MacRole).toString() == "aa:bb:cc:dd:ee:ff",
        "AssetModel direct DTO MAC should match");
    dtoAsset.insert("hostname", "dto-host-updated");
    assetModel.applyAssetDto(dtoAsset);
    expect(assetModel.data(assetModel.index(0, 0), asset_discovery::gui::AssetModel::HostnameRole).toString() == "dto-host-updated",
        "AssetModel direct DTO update should replace matching asset");

    asset_discovery::gui::LogModel logModel;
    QVariantMap dtoLog;
    dtoLog.insert("timestamp", "2026-01-01T00:00:00Z");
    dtoLog.insert("severity", "info");
    dtoLog.insert("source", "core");
    dtoLog.insert("message", "Capture started");
    logModel.loadLogDtos({dtoLog});
    expect(logModel.rowCount() == 1, "LogModel should load direct DTO logs");
    dtoLog.insert("message", "New packet batch");
    logModel.appendLogDto(dtoLog);
    expect(logModel.rowCount() == 2, "LogModel should append direct DTO logs");
    expect(logModel.data(logModel.index(0, 0), asset_discovery::gui::LogModel::MessageRole).toString() == "New packet batch",
        "LogModel append should insert newest first");

    const std::string testDb = "test_gui_models.db";
    std::remove(testDb.c_str());
    {
        asset_discovery::storage::SQLiteWriter writer(testDb);

        asset_discovery::asset::Asset asset;
        asset.macAddress = "00:11:22:33:44:55";
        asset.ipAddresses.insert("10.0.0.1");
        asset.firstSeen = {100, 200};
        asset.lastSeen = {100, 200};
        asset.sources.insert("arp");

        asset_discovery::asset::Asset activeAsset;
        activeAsset.macAddress = "66:77:88:99:aa:bb";
        activeAsset.ipAddresses.insert("10.0.0.2");
        const auto nowSeconds = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        activeAsset.firstSeen = {static_cast<long>(nowSeconds), 0};
        activeAsset.lastSeen = {static_cast<long>(nowSeconds), 0};
        activeAsset.sources.insert("dhcp");
        writer.writeAssets({asset, activeAsset});
    }

    assetModel.reloadFromDatabase(QString::fromStdString(testDb));
    expect(assetModel.rowCount() == 2, "AssetModel should load 2 assets from db");
    const QModelIndex idx = assetModel.index(0, 0);
    expect(assetModel.data(idx, asset_discovery::gui::AssetModel::MacRole).toString() == "00:11:22:33:44:55", "Asset MAC should match");
    expect(assetModel.data(idx, asset_discovery::gui::AssetModel::IpsRole).toStringList().contains("10.0.0.1"), "Asset IP list should contain 10.0.0.1");
    expect(assetModel.get(0).value("macAddress").toString() == "00:11:22:33:44:55", "Asset get should expose current row");
    expect(assetModel.exportToFile("test_assets_export.csv", "csv"), "Asset export should write CSV");
    expect(fileContains("test_assets_export.csv", "ip,mac,hostname,display_name,vendor,os_hint,device_type,model_hint,first_seen,last_seen,protocols"), "Asset CSV export should include core asset fields");
    expect(!assetModel.exportToFile("/proc/test_assets_export.csv", "csv"), "Asset export should report write failures");

    std::remove("test_assets_export.csv");
    std::remove(testDb.c_str());
}

void testUiNativeRunValidation()
{
    QSettings("PNAD", "PNAD Desktop").clear();
    asset_discovery::gui::CaptureController controller;
    controller.setSqlitePath("test_gui_validation.db");
    const std::string testPcap = "test_gui_validation.pcap";
    {
        std::ofstream file(testPcap, std::ios::binary);
        file.write("\xd4\xc3\xb2\xa1\x02\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x00\x00\x01\x00\x00\x00", 24);
    }

    controller.setPcapPath("");
    expectDebug(!controller.validatePcapAnalysisRequest(), "PCAP Analysis validation should require a PCAP file", controller.validationError().toStdString());
    expect(controller.validationError().toStdString().find("PCAP file") != std::string::npos,
        "PCAP Analysis validation should use product language");

    controller.setPcapPath("missing-capture.pcap");
    expectDebug(!controller.validatePcapAnalysisRequest(), "PCAP Analysis validation should reject a missing source file", controller.validationError().toStdString());
    expect(controller.validationError().toStdString().find("readable PCAP") != std::string::npos,
        "Missing PCAP validation should ask for a readable file");
    expect(!controller.isRunning(), "Controller should not enter running state for an invalid PCAP source");

    const std::string unsupportedFile = "test_gui_validation.txt";
    {
        std::ofstream file(unsupportedFile);
        file << "not a capture";
    }
    controller.setPcapPath(QString::fromStdString(unsupportedFile));
    expectDebug(!controller.validatePcapAnalysisRequest(), "PCAP Analysis validation should reject unsupported file extensions", controller.validationError().toStdString());
    expect(controller.validationError().toStdString().find("PCAPNG") != std::string::npos,
        "Unsupported source validation should mention supported PCAP formats");

    controller.setPcapPath(QString::fromStdString(testPcap));
    expectDebug(controller.validatePcapAnalysisRequest(), "PCAP Analysis validation should accept a selected PCAP file", controller.validationError().toStdString());

    controller.setPcapPath("");
    controller.setInterfaceName("test0");
    expectDebug(controller.validateLiveCaptureRequest(), "Live Capture config validation should not require a PCAP file", controller.validationError().toStdString());

    std::remove("test_gui_validation.db");
    std::remove(testPcap.c_str());
    std::remove(unsupportedFile.c_str());
}

void testSharedConfigValidation()
{
    QSettings("PNAD", "PNAD Desktop").clear();
    asset_discovery::gui::CaptureController controller;
    controller.setSqlitePath("test_gui_validation.db");
    const std::string testPcap = "test_gui_validation.pcap";
    {
        std::ofstream file(testPcap, std::ios::binary);
        file.write("\xd4\xc3\xb2\xa1\x02\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x00\x00\x01\x00\x00\x00", 24);
    }
    controller.setPcapPath(QString::fromStdString(testPcap));

    controller.setOutputFormat("xml");
    expect(!controller.validateSettings(), "Unsupported GUI output format should be rejected");
    const auto outputError = controller.validationError().toStdString();
    expect(outputError.find("format") != std::string::npos,
        "Validation error should mention unsupported export format");
    expect(outputError.find("--") == std::string::npos,
        "Desktop validation error should not expose command flags");

    std::remove("test_gui_validation.db");
    std::remove(testPcap.c_str());
}

void testPreferenceValidationAndRestore()
{
    QSettings("PNAD", "PNAD Desktop").clear();
    const std::string testDb = "test_gui_preferences.db";
    std::remove(testDb.c_str());

    qputenv("PNAD_EMAIL_ALERTS_ENABLED", "true");
    qputenv("PNAD_EMAIL_SMTP_HOST", "smtp.example.com");
    qputenv("PNAD_EMAIL_SMTP_PORT", "587");
    qputenv("PNAD_EMAIL_TLS_MODE", "starttls");
    qputenv("PNAD_EMAIL_USERNAME", "pnad");
    qputenv("PNAD_EMAIL_PASSWORD_ENV", "PNAD_SMTP_PASSWORD");
    qputenv("PNAD_EMAIL_FROM", "pnad@example.com");

    asset_discovery::gui::CaptureController controller;
    controller.setSqlitePath(QString::fromStdString(testDb));
    controller.setPacketFilter("arp");
    controller.setCaptureBackend("auto");
    controller.setOutputFormat("csv");
    controller.setEmailRecipients("admin@example.com");
    expectDebug(controller.validatePreferences(), "Valid Preferences should pass validation", controller.validationError().toStdString());
    controller.saveSettingsToDb();

    asset_discovery::gui::CaptureController restored;
    restored.setSqlitePath(QString::fromStdString(testDb));
    restored.loadSettingsFromDb();
    expect(restored.packetFilter().toStdString() == "arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353", "Preferences should keep fixed core ARP/DHCP/SSDP/mDNS filter");
    expect(restored.outputFormat().toStdString() == "csv", "Preferences should restore export format");
    expect(restored.emailAlertsEnabled(), "Email enablement should come from environment");
    expect(restored.emailSmtpHost().toStdString() == "smtp.example.com", "SMTP host should come from environment");
    expect(restored.emailRecipients().toStdString() == "admin@example.com", "Preferences should restore email recipients");

    controller.setCaptureBackend("af-packet");
    expect(controller.captureBackend().toStdString() == "auto", "Removed backend preferences should fall back to auto");
    controller.setCaptureBackend("");
    expect(controller.captureBackend().toStdString() == "auto", "Empty backend preferences should fall back to auto");

    controller.setSqlitePath("/proc/pnad.db");
    expectDebug(!controller.validatePreferences(), "Preferences should reject unwritable local database paths", controller.validationError().toStdString());
    expect(controller.validationError().toStdString().find("writable local database") != std::string::npos,
        "Invalid database validation should explain writable storage");

    qunsetenv("PNAD_EMAIL_ALERTS_ENABLED");
    qunsetenv("PNAD_EMAIL_SMTP_HOST");
    qunsetenv("PNAD_EMAIL_SMTP_PORT");
    qunsetenv("PNAD_EMAIL_TLS_MODE");
    qunsetenv("PNAD_EMAIL_USERNAME");
    qunsetenv("PNAD_EMAIL_PASSWORD_ENV");
    qunsetenv("PNAD_EMAIL_FROM");

    std::remove(testDb.c_str());
}

void testEmailAlertNotifier()
{
    using asset_discovery::gui::EmailAlertNotifier;
    using asset_discovery::gui::EmailAlertSettings;

    EmailAlertSettings disabled;
    expect(!EmailAlertNotifier::validateSettings(disabled).has_value(), "Disabled email alerts should not require SMTP settings");

    EmailAlertSettings invalid;
    invalid.enabled = true;
    expect(EmailAlertNotifier::validateSettings(invalid).has_value(), "Enabled email alerts should require delivery settings");

    const auto event = sampleNewAssetEvent();
    auto settings = validEmailSettings();
    const auto message = EmailAlertNotifier::formatNewAssetMessage(event, settings);
    expect(message.subject.toStdString().find("aa:bb:cc:dd:ee:ff") != std::string::npos,
        "Email subject should include MAC address");
    expect(message.body.toStdString().find("192.168.1.23") != std::string::npos,
        "Email body should include IP address");
    expect(message.body.toStdString().find("laptop-user") != std::string::npos,
        "Email body should include hostname");
    expect(message.body.toStdString().find("dhcp") != std::string::npos,
        "Email body should include protocol");
    expect(message.body.toStdString().find("eth0") != std::string::npos,
        "Email body should include interface");

    auto fakeSender = std::make_unique<FakeMailSender>();
    auto* fakeSenderPtr = fakeSender.get();
    EmailAlertNotifier notifier(settings, std::move(fakeSender));
    expect(notifier.handleAssetEvent(event), "Notifier should enqueue first new asset event");
    expect(!notifier.handleAssetEvent(event), "Notifier should suppress duplicate asset event in a session");
    for (int i = 0; i < 100 && fakeSenderPtr->messages.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    notifier.stop();
    expect(fakeSenderPtr->messages.size() == 1, "Notifier should send one email after duplicate suppression");

    auto failingSender = std::make_unique<FakeMailSender>();
    failingSender->failure = "SMTP failed";
    EmailAlertNotifier failingNotifier(settings, std::move(failingSender));
    bool failureReported = false;
    failingNotifier.setFailureCallback([&](const QString& message) {
        failureReported = message.contains("SMTP failed");
    });
    failingNotifier.handleAssetEvent(event);
    for (int i = 0; i < 100 && !failureReported; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    failingNotifier.stop();
    expect(failureReported, "Notifier should report delivery failures");
}

void testPcapAnalysisPersistsAssets()
{
    QSettings("PNAD", "PNAD Desktop").clear();
    const std::string testDb = "test_gui_pcap_session.db";
    std::remove(testDb.c_str());

    asset_discovery::gui::CaptureController controller;
    controller.setSqlitePath(QString::fromStdString(testDb));
    controller.setPcapPath(QString(PNAD_SOURCE_DIR) + "/samples/multi-asset.pcap");
    controller.setPacketFilter("arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353");

    controller.startPcapAnalysis();
    for (int i = 0; i < 300; ++i) {
        if (!controller.isRunning() && controller.statusText() != "Stopped") {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    expectDebug(!controller.isRunning(), "PCAP Analysis should finish in the controller test", controller.lastError().toStdString());
    expectDebug(controller.lastError().isEmpty(), "PCAP Analysis should finish without controller error", controller.lastError().toStdString());

    asset_discovery::gui::AssetModel assets;
    assets.reloadFromDatabase(QString::fromStdString(testDb));
    expect(assets.rowCount() > 0, "PCAP Analysis should persist discovered assets");

    asset_discovery::gui::AssetModel restoredAssets;
    restoredAssets.reloadFromDatabase(QString::fromStdString(testDb));
    expect(restoredAssets.rowCount() == assets.rowCount(), "Restart restore should reload persisted assets");

    std::remove(testDb.c_str());
}

void testInterfaceModel()
{
    QSettings("PNAD", "PNAD Desktop").clear();
    asset_discovery::gui::InterfaceModel model;
    model.refresh();

    if (model.rowCount() > 0) {
        const QString systemName = model.systemNameAt(0);
        expect(!systemName.isEmpty(), "Interface model systemNameAt should return a name");
        expect(model.findBySystemName(systemName) == 0, "Interface model should find the first interface by system name");

        const auto row = model.get(0);
        expect(row.contains("systemName"), "Interface row map should include systemName");
        expect(row.contains("addresses"), "Interface row map should include addresses");

        asset_discovery::gui::CaptureController controller;
        controller.setInterfaceName(systemName);
        expect(controller.interfaceName() == systemName, "Controller should accept exact interface system name from picker");
    }
}

} // namespace

int main()
{
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, ".");
    QSettings("PNAD", "PNAD Desktop").clear();

    testCoreGuiModelsAndController();
    testUiNativeRunValidation();
    testSharedConfigValidation();
    testPreferenceValidationAndRestore();
    testEmailAlertNotifier();
    testPcapAnalysisPersistsAssets();
    testInterfaceModel();

    if (failures > 0) {
        std::cerr << failures << " GUI Model test expectation(s) failed\n";
        return 1;
    }
    std::cout << "All core GUI model/controller tests passed successfully!\n";
    return 0;
}
