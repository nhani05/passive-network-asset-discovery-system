import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: window
    visible: true
    width: 1240
    height: 820
    title: "PNAD Desktop"

    readonly property color colorBg: "#111827"
    readonly property color colorSidebar: "#17202b"
    readonly property color colorCard: "#1f2937"
    readonly property color colorPanel: "#243244"
    readonly property color colorBorder: "#3b4758"
    readonly property color colorTextMain: "#f9fafb"
    readonly property color colorTextMuted: "#a7b0bf"
    readonly property color colorAccent: "#2dd4bf"
    readonly property color colorHigh: "#f43f5e"
    readonly property color colorWarn: "#fbbf24"
    readonly property color colorInfo: "#22c55e"

    property string selectedAssetIdentity: ""
    property var smokeSources: [
        "LiveCaptureView.qml",
        "DashboardView.qml",
        "AssetInventoryView.qml",
        "AssetDetailView.qml",
        "SecurityCenterView.qml",
        "PcapAnalysisView.qml",
        "ReportsExportView.qml",
        "PreferencesView.qml",
        "SystemHealthView.qml"
    ]
    property int smokeIndex: 0

    background: Rectangle { color: window.colorBg }

    Timer {
        id: refreshTimer
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            if (captureController.isRunning) {
                reloadDataModels();
            }
        }
    }

    Connections {
        target: captureController
        function onCaptureFinished() {
            reloadDataModels();
        }
        function onSqlitePathChanged() {
            reloadDataModels();
        }
    }

    function reloadDataModels() {
        assetModel.reloadFromDatabase(captureController.sqlitePath);
        eventModel.reloadFromDatabase(captureController.sqlitePath);
        analysisSessionModel.reloadFromDatabase(captureController.sqlitePath);
    }

    function hasArgument(value) {
        for (var i = 0; i < Qt.application.arguments.length; ++i) {
            if (Qt.application.arguments[i] === value) {
                return true;
            }
        }
        return false;
    }

    function initialSource() {
        return assetModel.rowCount() === 0 && analysisSessionModel.completedSessionCount === 0
               ? "LiveCaptureView.qml"
               : "DashboardView.qml";
    }

    function setCheckedForSource(source) {
        btnDashboard.checked = source === "DashboardView.qml";
        btnLive.checked = source === "LiveCaptureView.qml";
        btnAssets.checked = source === "AssetInventoryView.qml" || source === "AssetDetailView.qml";
        btnEvents.checked = source === "SecurityCenterView.qml";
        btnPcap.checked = source === "PcapAnalysisView.qml";
        btnReports.checked = source === "ReportsExportView.qml";
        btnSettings.checked = source === "PreferencesView.qml";
        btnHealth.checked = source === "SystemHealthView.qml";
    }

    function setNavSource(source) {
        viewLoader.source = source;
        setCheckedForSource(source);
    }

    function showLiveCapture() {
        setNavSource("LiveCaptureView.qml");
    }

    function showPcapAnalysis() {
        setNavSource("PcapAnalysisView.qml");
    }

    function showCaptureCenter(mode) {
        if (mode === 1) {
            showPcapAnalysis();
        } else {
            showLiveCapture();
        }
    }

    function showAssetsForQuery(query) {
        selectedAssetIdentity = query;
        setNavSource("AssetInventoryView.qml");
        if (viewLoader.item && viewLoader.item.setSearchQuery) {
            viewLoader.item.setSearchQuery(query);
        }
    }

    function showAssetDetail(identity) {
        selectedAssetIdentity = identity;
        setNavSource("AssetDetailView.qml");
    }

    function showEventDetail(row) {
        setNavSource("SecurityCenterView.qml");
        if (viewLoader.item && viewLoader.item.openEventRow) {
            viewLoader.item.openEventRow(row);
        }
    }

    Timer {
        id: smokeTimer
        interval: 120
        repeat: true
        onTriggered: {
            if (window.smokeIndex >= window.smokeSources.length) {
                Qt.quit();
                return;
            }
            if (window.smokeSources[window.smokeIndex] === "AssetDetailView.qml"
                && window.selectedAssetIdentity === "") {
                window.selectedAssetIdentity = "smoke-test-missing-asset";
            }
            window.setNavSource(window.smokeSources[window.smokeIndex]);
            window.smokeIndex += 1;
        }
    }

    Component.onCompleted: {
        reloadDataModels();
        setNavSource(initialSource());
        if (hasArgument("--smoke-first-run")
            && viewLoader.source.toString().indexOf("LiveCaptureView.qml") === -1) {
            console.error("FirstRunAssertionFailed: expected Live Capture, got " + viewLoader.source);
            Qt.quit();
            return;
        }
        if (hasArgument("--smoke-cycle")) {
            smokeTimer.start();
        }
    }

    component NavButton: Button {
        Layout.fillWidth: true
        checkable: true
        ButtonGroup.group: navGroup
        background: Rectangle {
            color: parent.checked ? window.colorPanel : "transparent"
            radius: 6
        }
        contentItem: Text {
            text: parent.text
            color: parent.checked ? window.colorAccent : window.colorTextMain
            font.bold: parent.checked
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            leftPadding: 12
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 246
            color: window.colorSidebar

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: captureController.isRunning ? window.colorInfo : window.colorTextMuted
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text {
                            text: "PNAD Desktop"
                            font.pixelSize: 18
                            font.bold: true
                            color: window.colorTextMain
                        }
                        Text {
                            text: captureController.isRunning ? "Capture running" : "Ready"
                            font.pixelSize: 11
                            color: window.colorTextMuted
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: window.colorBorder }

                ButtonGroup { id: navGroup }

                NavButton {
                    id: btnDashboard
                    text: "Dashboard"
                    onClicked: window.setNavSource("DashboardView.qml")
                }
                NavButton {
                    id: btnLive
                    text: "Live Capture"
                    onClicked: window.showLiveCapture()
                }
                NavButton {
                    id: btnAssets
                    text: "Asset Inventory"
                    onClicked: window.setNavSource("AssetInventoryView.qml")
                }
                NavButton {
                    id: btnEvents
                    text: "Security Events"
                    onClicked: window.setNavSource("SecurityCenterView.qml")
                }
                NavButton {
                    id: btnPcap
                    text: "PCAP Analysis"
                    onClicked: window.showPcapAnalysis()
                }
                NavButton {
                    id: btnReports
                    text: "Reports / Export"
                    onClicked: window.setNavSource("ReportsExportView.qml")
                }

                Item { Layout.fillHeight: true }

                NavButton {
                    id: btnSettings
                    text: "Settings"
                    onClicked: window.setNavSource("PreferencesView.qml")
                }
                NavButton {
                    id: btnHealth
                    text: "System Health"
                    onClicked: window.setNavSource("SystemHealthView.qml")
                }
            }
        }

        Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: window.colorBorder }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Error banner — visible only when there is a capture error
            Rectangle {
                id: errorBanner
                Layout.fillWidth: true
                visible: captureController.lastError !== ""
                // Height tracks the inner Column's actual rendered height + 16px padding
                height: visible ? (bannerTextCol.height + 16) : 0
                color: "#4c1d2f"
                border.color: window.colorHigh
                border.width: 1
                clip: true

                property bool bannerIsPermissionError:
                    captureController.lastError.indexOf("permission") !== -1 ||
                    captureController.lastError.indexOf("Permission") !== -1 ||
                    captureController.lastError.indexOf("CAP_NET_RAW") !== -1 ||
                    captureController.lastError.indexOf("don't have permission") !== -1

                // Dismiss button — top-right corner, fixed size
                Button {
                    id: dismissBtn
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 8
                    anchors.rightMargin: 12
                    width: 72
                    height: 28
                    text: "Dismiss"
                    onClicked: captureController.clearError()
                    background: Rectangle {
                        color: "transparent"
                        border.color: "#fecdd3"
                        border.width: 1
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#ffffff"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // Text column — drives the banner height, wraps freely
                Column {
                    id: bannerTextCol
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: dismissBtn.left
                    anchors.topMargin: 8
                    anchors.leftMargin: 14
                    anchors.rightMargin: 10
                    spacing: 4

                    Text {
                        width: parent.width
                        text: "\u26A0  " + captureController.lastError
                        color: "#ffe4e6"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        width: parent.width
                        visible: errorBanner.bannerIsPermissionError
                        height: visible ? implicitHeight : 0
                        text: "Fix: sudo setcap cap_net_raw,cap_net_admin=eip " + Qt.application.arguments[0]
                        color: "#fda4af"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 48
                color: window.colorSidebar
                border.color: window.colorBorder
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 18

                    Text {
                        text: captureController.isRunning
                              ? "RUNNING"
                              : (captureController.lastError === "" ? "READY" : "ATTENTION")
                        color: captureController.isRunning
                               ? window.colorInfo
                               : (captureController.lastError === "" ? window.colorTextMain : window.colorHigh)
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Text {
                        text: captureController.isLive
                              ? ("Interface: " + (captureController.interfaceName === "" ? "Not selected" : captureController.interfaceName))
                              : ("PCAP: " + (captureController.pcapPath === "" ? "Not selected" : captureController.pcapPath))
                        color: window.colorTextMuted
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                        Layout.maximumWidth: 330
                    }
                    Text {
                        text: "Assets: " + assetModel.rowCount()
                        color: window.colorTextMain
                        font.pixelSize: 12
                    }
                    Text {
                        text: "High events: " + eventModel.highSeverityCount
                        color: eventModel.highSeverityCount > 0 ? window.colorHigh : window.colorTextMain
                        font.pixelSize: 12
                        font.bold: eventModel.highSeverityCount > 0
                    }
                    Text {
                        text: "Database: " + (captureController.sqlitePath === "" ? "Not configured" : "Connected")
                        color: captureController.sqlitePath === "" ? window.colorWarn : window.colorTextMain
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "Refresh"
                        onClicked: reloadDataModels()
                        background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 4 }
                        contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 6 }
                    }
                }
            }

            Loader {
                id: viewLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Rectangle {
                Layout.fillWidth: true
                height: 32
                color: window.colorSidebar
                border.color: window.colorBorder
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    Text {
                        text: captureController.isRunning
                              ? "Product state: running " + (captureController.isLive ? "Live Capture" : "PCAP Analysis")
                              : "Product state: " + captureController.statusText
                        color: window.colorTextMuted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.maximumWidth: 600
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Storage: " + captureController.sqlitePath
                        color: window.colorTextMuted
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                        Layout.maximumWidth: 520
                    }
                }
            }
        }
    }
}
