#include "pnad/gui/AssetModel.hpp"
#include "pnad/gui/EventModel.hpp"
#include "pnad/gui/InterfaceModel.hpp"
#include "pnad/gui/AnalysisSessionModel.hpp"
#include "pnad/gui/HealthDiagnosticsModel.hpp"
#include "pnad/gui/CaptureController.hpp"
#include "pnad/gui/CaptureServiceFacade.hpp"
#include "pnad/gui/DesktopRunConfig.hpp"
#include "pnad/storage/SQLiteWriter.hpp"
#include <iostream>
#include <string>
#include <cstdio>
#include <fstream>
#include <QSettings>
#include <thread>
#include <chrono>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
    }
}

void expectDebug(bool condition, const std::string& message, const std::string& debugInfo)
{
    if (!condition) {
        ++failures;
    }
}

bool fileContains(const std::string& path, const std::string& needle)
{
    std::ifstream file(path);
    const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return contents.find(needle) != std::string::npos;
}

void testGuiModelsAndController()
{
    // 1. Verify CaptureController defaults
    asset_discovery::gui::CaptureController controller;
    asset_discovery::gui::CaptureServiceFacade service(&controller);
    expect(controller.isLive() == true, "Default mode should be live");
    expect(service.sqlitePath() == controller.sqlitePath(), "Capture service facade should expose the same database path");
    expect(service.serviceStatus() == "stopped", "Capture service facade should report a stopped state by default");
    expect(service.backendExecutablePath().contains("asset-discovery-backend"), "Capture service facade should expose the backend executable path");
    service.stopCapture();
    expect(!service.isRunning(), "Capture service facade should reflect stop state");
    expect(controller.packetFilter().toStdString() == "arp or udp port 67 or udp port 68", "Default BPF filter");
    expect(!controller.sqlitePath().isEmpty(), "Default database path should be resolved");

    // 2. Verify AssetModel and EventModel initially empty
    asset_discovery::gui::AssetModel assetModel;
    asset_discovery::gui::EventModel eventModel;
    expect(assetModel.rowCount() == 0, "AssetModel should be initially empty");
    expect(eventModel.rowCount() == 0, "EventModel should be initially empty");

    // 3. Create dummy database and populate it
    std::string testDb = "test_gui_models.db";
    std::remove(testDb.c_str());

    {
        asset_discovery::storage::SQLiteWriter writer(testDb);

        // Write an asset
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

        // Write an event
        asset_discovery::asset::AssetEvent event;
        event.timestamp = {100, 200};
        event.type = asset_discovery::asset::AssetEventType::NewAsset;
        event.severity = asset_discovery::asset::AssetEventSeverity::Info;
        event.ipAddress = "10.0.0.1";
        event.macAddress = "00:11:22:33:44:55";
        event.protocol = "arp";
        event.interfaceName = "eth0";
        event.message = "Test message";
        writer.write(event);

        asset_discovery::asset::AssetEvent highEvent;
        highEvent.timestamp = {101, 300};
        highEvent.type = asset_discovery::asset::AssetEventType::MacChangedForIp;
        highEvent.severity = asset_discovery::asset::AssetEventSeverity::High;
        highEvent.ipAddress = "10.0.0.1";
        highEvent.macAddress = "00:11:22:33:44:55";
        highEvent.oldMacAddress = "aa:bb:cc:dd:ee:ff";
        highEvent.newMacAddress = "00:11:22:33:44:55";
        highEvent.protocol = "arp";
        highEvent.interfaceName = "eth0";
        highEvent.message = "IP used by new MAC";
        writer.write(highEvent);
    }

    // 4. Reload models from the database
    assetModel.reloadFromDatabase(QString::fromStdString(testDb));
    eventModel.reloadFromDatabase(QString::fromStdString(testDb));

    expect(assetModel.rowCount() == 2, "AssetModel should load 2 assets from db");
    expect(eventModel.rowCount() == 2, "EventModel should load 2 events from db");

    // Check roles
    QModelIndex idx = assetModel.index(0, 0);
    expect(assetModel.data(idx, asset_discovery::gui::AssetModel::MacRole).toString() == "00:11:22:33:44:55", "Asset MAC should match");
    expect(assetModel.data(idx, asset_discovery::gui::AssetModel::IpsRole).toStringList().contains("10.0.0.1"), "Asset IP list should contain 10.0.0.1");
    expect(assetModel.matchesSearch(0, "00:11"), "Asset search should match MAC address");
    expect(assetModel.matchesSearch(0, "10.0.0.1"), "Asset search should match IP address");
    expect(assetModel.matchesSearch(0, "arp"), "Asset search should match discovery source");
    expect(!assetModel.matchesSearch(0, "does-not-exist"), "Asset search should reject unrelated query");
    expect(assetModel.data(idx, asset_discovery::gui::AssetModel::StatusRole).toString() == "Offline", "Old asset should be offline");
    expect(assetModel.data(idx, asset_discovery::gui::AssetModel::RiskRole).toString() == "High", "High severity related event should drive asset risk");
    expect(assetModel.matchesFilters(0, "10.0.0.1", "Offline", "High", "arp"), "Asset filters should match status, risk, source, and query");
    expect(!assetModel.matchesFilters(0, "10.0.0.1", "Active", "High", "arp"), "Asset filters should reject mismatched status");
    expect(assetModel.networkGroupForRow(0) == "10.0.0.0/24", "Network Map grouping should derive deterministic subnet");
    expect(assetModel.activeAssetCount() == 1, "Active asset count should derive from recent last-seen timestamps");
    expect(assetModel.newAssetsTodayCount() == 1, "New assets today should derive from first-seen timestamps");
    expect(assetModel.highRiskAssetCount() == 1, "High risk asset count should derive from related event severity");
    expect(assetModel.newestAssetLabel().contains("66:77:88:99:aa:bb"), "Newest asset label should prefer latest first-seen asset");
    expect(assetModel.assetForIdentity("10.0.0.1").value("macAddress").toString() == "00:11:22:33:44:55", "Asset identity lookup should work by IP");
    expect(assetModel.timelineForAsset("00:11:22:33:44:55").size() >= 3, "Asset timeline should include first seen, events, and last seen");
    assetModel.sortByLastSeenDescending();
    expect(assetModel.data(assetModel.index(0, 0), asset_discovery::gui::AssetModel::MacRole).toString() == "66:77:88:99:aa:bb",
        "Last-seen sort should move newest asset first");
    assetModel.sortByMacAddress();

    QModelIndex evIdx = eventModel.index(0, 0);
    expect(eventModel.data(evIdx, asset_discovery::gui::EventModel::MacAddressRole).toString() == "00:11:22:33:44:55", "Event MAC should match");
    expect(eventModel.data(evIdx, asset_discovery::gui::EventModel::MessageRole).toString() == "IP used by new MAC", "Newest event message should match");
    expect(eventModel.matchesFilter(0, "eth0", "all"), "Event search should match interface");
    expect(eventModel.matchesFilter(0, "Mac Changed", "high"), "Event search should match readable type label and severity");
    expect(!eventModel.matchesFilter(0, "eth0", "info"), "Event severity filter should reject other severity");
    expect(eventModel.relatedAssetQuery(0) == "00:11:22:33:44:55", "Event pivot query should prefer related MAC");
    expect(eventModel.highSeverityCount() == 1, "Event high severity count should derive from loaded events");
    expect(eventModel.unresolvedCount() == eventModel.rowCount(), "Unresolved count should equal all events when persistence is not implemented");
    expect(eventModel.readableEventType(0) == "Mac Changed For Ip", "Event readable label should be title-cased from raw event type");

    expect(assetModel.exportToFile("test_assets_export.csv", "csv"), "Asset export should write CSV");
    expect(eventModel.exportToFile("test_events_export.json", "json"), "Event export should write JSON");
    expect(eventModel.exportEvidenceToFile(0, "test_event_evidence.csv", "csv"), "Event evidence export should write CSV");
    expect(fileContains("test_assets_export.csv", "status,risk"), "Asset CSV export should include status and risk fields");
    expect(fileContains("test_events_export.json", "\"metadata\""), "Event JSON export should include evidence metadata");
    expect(fileContains("test_event_evidence.csv", "event_evidence") || fileContains("test_event_evidence.csv", "metadata"),
        "Event evidence export should include evidence fields");
    expect(!assetModel.exportToFile("/proc/test_assets_export.csv", "csv"), "Asset export should report write failures");
    std::remove("test_assets_export.csv");
    std::remove("test_events_export.json");
    std::remove("test_event_evidence.csv");

    // 5. Test CaptureController settings save/load
    {
        asset_discovery::gui::CaptureController settingsController;
        settingsController.setSqlitePath(QString::fromStdString(testDb));
        settingsController.setInterfaceName("eth99");
        settingsController.setPacketFilter("udp");
        settingsController.setConfigPath("configs/default.yaml");
        settingsController.setProfileName("");
        settingsController.setOutputFormat("csv");
        settingsController.setEventRateLimitSeconds(123);
        settingsController.saveSettingsToDb();

        asset_discovery::gui::CaptureController reloadController;
        reloadController.setSqlitePath(QString::fromStdString(testDb));
        reloadController.loadSettingsFromDb();

        expect(reloadController.interfaceName().toStdString() == "eth99", "Reloaded interfaceName should match");
        expect(reloadController.packetFilter().toStdString() == "udp", "Reloaded packetFilter should match");
        expect(reloadController.configPath().toStdString() == "configs/default.yaml", "Reloaded configPath should match");
        expect(reloadController.outputFormat().toStdString() == "csv", "Reloaded outputFormat should match");
        expect(reloadController.eventRateLimitSeconds() == 123, "Reloaded eventRateLimitSeconds should match");
    }

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

    controller.setInterfaceName("");
    expectDebug(!controller.validateLiveCaptureRequest(), "Live Capture validation should require an interface", controller.validationError().toStdString());
    expect(controller.validationError().toStdString().find("network interface") != std::string::npos,
        "Live Capture validation should use product language");

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

    controller.setInterfaceName("eth0");
    controller.setPcapPath(QString::fromStdString(testPcap));
    expectDebug(controller.validateLiveCaptureRequest(), "Live Capture validation should accept a selected interface", controller.validationError().toStdString());
    expectDebug(controller.validatePcapAnalysisRequest(), "PCAP Analysis validation should accept a selected PCAP file", controller.validationError().toStdString());

    std::remove("test_gui_validation.db");
    std::remove(testPcap.c_str());
    std::remove(unsupportedFile.c_str());
}

void testSharedConfigValidation()
{
    QSettings("PNAD", "PNAD Desktop").clear();
    asset_discovery::gui::CaptureController controller;
    controller.setSqlitePath("test_gui_validation.db");
    controller.setInterfaceName("eth0");
    controller.setConfigPath("configs/default.yaml");
    controller.setProfileName("live");

    expect(!controller.validateSettings(), "Config/profile mutual exclusion should be rejected");
    expect(controller.validationError().toStdString().find("preferences file or a preset") != std::string::npos,
        "Validation error should use desktop preferences language");

    controller.setProfileName("");
    controller.setConfigPath("");
    controller.setOutputFormat("xml");
    expect(!controller.validateSettings(), "Unsupported GUI output format should be rejected");
    const auto outputError = controller.validationError().toStdString();
    expect(outputError.find("format") != std::string::npos,
        "Validation error should mention unsupported export format, got: " + outputError);
    expect(outputError.find("--") == std::string::npos,
        "Desktop validation error should not expose command flags, got: " + outputError);

    std::remove("test_gui_validation.db");
}

void testPreferenceValidationAndRestore()
{
    QSettings("PNAD", "PNAD Desktop").clear();
    const std::string testDb = "test_gui_preferences.db";
    std::remove(testDb.c_str());

    asset_discovery::gui::CaptureController controller;
    controller.setSqlitePath(QString::fromStdString(testDb));
    controller.setPacketFilter("arp");
    controller.setCaptureBackend("auto");
    controller.setOutputFormat("csv");
    controller.setLocalNetworks(QStringList() << "192.168.1.0/24");
    controller.setIgnoredNetworks(QStringList() << "10.0.0.0/8");
    expectDebug(controller.validatePreferences(), "Valid Preferences should pass validation", controller.validationError().toStdString());
    controller.saveSettingsToDb();

    asset_discovery::gui::CaptureController restored;
    restored.setSqlitePath(QString::fromStdString(testDb));
    restored.loadSettingsFromDb();
    expect(restored.packetFilter().toStdString() == "arp", "Preferences should restore capture filter");
    expect(restored.outputFormat().toStdString() == "csv", "Preferences should restore export format");
    expect(restored.localNetworks().contains("192.168.1.0/24"), "Preferences should restore local networks");

    controller.setLocalNetworks(QStringList() << "192.168.1.0");
    expectDebug(!controller.validatePreferences(), "Preferences should reject invalid local network CIDR values", controller.validationError().toStdString());
    expect(controller.validationError().toStdString().find("valid IPv4 CIDR") != std::string::npos,
        "Invalid network validation should use product language");

    controller.setLocalNetworks(QStringList() << "192.168.1.0/24");
    controller.setSqlitePath("/proc/pnad.db");
    expectDebug(!controller.validatePreferences(), "Preferences should reject unwritable local database paths", controller.validationError().toStdString());
    expect(controller.validationError().toStdString().find("writable local database") != std::string::npos,
        "Invalid database validation should explain writable storage");

    std::remove(testDb.c_str());
}

void testHealthDiagnosticsModel()
{
    QSettings("PNAD", "PNAD Desktop").clear();
    asset_discovery::gui::HealthDiagnosticsModel model;
    model.refresh(
        "/proc/pnad-health.db",
        "",
        "auto",
        "worker failed",
        "Error",
        "worker failed",
        "logs/pnad-runtime.log");

    expect(model.rowCount() >= 6, "Health diagnostics should expose product readiness categories");

    auto rowForCategory = [&model](const QString& category) {
        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row, 0);
            if (model.data(index, asset_discovery::gui::HealthDiagnosticsModel::CategoryRole).toString() == category) {
                return row;
            }
        }
        return -1;
    };

    const int selectedInterfaceRow = rowForCategory("Selected interface");
    expect(selectedInterfaceRow >= 0, "Health diagnostics should include selected interface readiness");
    expect(model.data(model.index(selectedInterfaceRow, 0), asset_discovery::gui::HealthDiagnosticsModel::StateRole).toString() == "Warning",
        "Missing selected interface should be a warning");

    const int databaseRow = rowForCategory("Local database");
    expect(databaseRow >= 0, "Health diagnostics should include local database readiness");
    expect(model.data(model.index(databaseRow, 0), asset_discovery::gui::HealthDiagnosticsModel::StateRole).toString() == "Error",
        "Unwritable database path should be an error");
    expect(!model.data(model.index(databaseRow, 0), asset_discovery::gui::HealthDiagnosticsModel::RemediationRole).toString().isEmpty(),
        "Database health issue should include remediation");

    const int runtimeRow = rowForCategory("Runtime status");
    expect(runtimeRow >= 0, "Health diagnostics should include runtime status");
    expect(model.data(model.index(runtimeRow, 0), asset_discovery::gui::HealthDiagnosticsModel::StateRole).toString() == "Error",
        "Runtime worker failure should be an error");
    expect(model.data(model.index(runtimeRow, 0), asset_discovery::gui::HealthDiagnosticsModel::DetailRole).toString().contains("worker failed"),
        "Runtime worker failure should be visible");

    const int logRow = rowForCategory("Runtime log");
    expect(logRow >= 0, "Health diagnostics should expose runtime log location");
    expect(model.data(model.index(logRow, 0), asset_discovery::gui::HealthDiagnosticsModel::StateRole).toString() == "OK",
        "Configured runtime log path should be OK");

    expect(rowForCategory("Parser") >= 0, "Health diagnostics should include parser readiness");
    expect(rowForCategory("Parser queue") >= 0, "Health diagnostics should include parser queue visibility");
    expect(rowForCategory("Dropped counters") >= 0, "Health diagnostics should include dropped counter visibility");
    expect(rowForCategory("Resource usage") >= 0, "Health diagnostics should include resource usage visibility");
}

void testPcapAnalysisCreatesSessionAndRestoresData()
{
    QSettings("PNAD", "PNAD Desktop").clear();
    const std::string testDb = "test_gui_pcap_session.db";
    std::remove(testDb.c_str());

    asset_discovery::gui::CaptureController controller;
    controller.setSqlitePath(QString::fromStdString(testDb));
    controller.setPcapPath(QString(PNAD_SOURCE_DIR) + "/samples/multi-asset.pcap");
    controller.setPacketFilter("arp or udp port 67 or udp port 68");

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
    asset_discovery::gui::EventModel events;
    asset_discovery::gui::AnalysisSessionModel sessions;
    assets.reloadFromDatabase(QString::fromStdString(testDb));
    events.reloadFromDatabase(QString::fromStdString(testDb));
    sessions.reloadFromDatabase(QString::fromStdString(testDb));

    expect(sessions.completedSessionCount() >= 1, "PCAP Analysis should create a completed analysis session");
    expect(assets.rowCount() > 0, "PCAP Analysis should persist discovered assets");
    expect(events.rowCount() > 0, "PCAP Analysis should persist events");
    expect(sessions.latestAssetCount() == assets.rowCount(), "Latest session asset count should match persisted inventory");
    expect(sessions.latestEventCount() == events.rowCount(), "Latest session event count should match persisted events");
    expect(!sessions.latestSessionLabel().isEmpty(), "Latest session label should summarize mode and status");
    expect(sessions.exportSummaryToFile("test_gui_session_summary.csv", "csv"), "Session summary export should write CSV");
    expect(fileContains("test_gui_session_summary.csv", "dropped_counters"), "Session summary export should include unavailable dropped counter field");

    asset_discovery::gui::AssetModel restoredAssets;
    asset_discovery::gui::AnalysisSessionModel restoredSessions;
    restoredAssets.reloadFromDatabase(QString::fromStdString(testDb));
    restoredSessions.reloadFromDatabase(QString::fromStdString(testDb));
    expect(restoredAssets.rowCount() == assets.rowCount(), "Restart restore should reload persisted assets");
    expect(restoredSessions.completedSessionCount() == sessions.completedSessionCount(), "Restart restore should reload session history");

    std::remove(testDb.c_str());
    std::remove("test_gui_session_summary.csv");
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

    testGuiModelsAndController();
    testUiNativeRunValidation();
    testSharedConfigValidation();
    testPreferenceValidationAndRestore();
    testHealthDiagnosticsModel();
    testPcapAnalysisCreatesSessionAndRestoresData();
    testInterfaceModel();

    if (failures > 0) {
        std::cerr << failures << " GUI Model test expectation(s) failed\n";
        return 1;
    }
    std::cout << "All GUI Model and Controller tests passed successfully!\n";
    return 0;
}
