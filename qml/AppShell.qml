import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: shell
    anchors.fill: parent

    property int currentPage: 0
    property int selectedRow: -1
    property var selectedAsset: ({})
    property string searchQuery: ""
    property string exportFormat: "json"
    property string exportStatus: ""
    property int smokePageIndex: 0
    readonly property string fixedFilter: "arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353"
    readonly property string supportedProtocols: "ARP, DHCP, DNS, mDNS, LLMNR, NetBIOS, SSDP, TCP"

    readonly property var pages: [
        { title: "Dashboard" },
        { title: "Capture" },
        { title: "Assets" },
        { title: "Events" },
        { title: "Settings" }
    ]

    function runStateText() {
        return captureController.isRunning ? "RUNNING"
             : (captureController.lastError === "" ? "STOPPED" : "ERROR");
    }

    function runStateColor() {
        return captureController.isRunning ? window.colorInfo
             : (captureController.lastError === "" ? window.colorTextMuted : window.colorHigh);
    }

    function currentSourceLabel() {
        if (captureController.isLive) {
            return captureController.interfaceName === "" ? "Live interface not selected"
                                                           : "Live: " + captureController.interfaceName;
        }
        return captureController.pcapPath === "" ? "PCAP file not selected"
                                                 : "PCAP: " + captureController.pcapPath;
    }

    function primaryIp(asset) {
        if (!asset || !asset.ipAddresses || asset.ipAddresses.length === 0) {
            return "-";
        }
        return asset.ipAddresses.join(", ");
    }

    function protocols(asset) {
        if (!asset || !asset.discoverySources || asset.discoverySources.length === 0) {
            return "-";
        }
        return asset.discoverySources.join(", ");
    }

    function pad2(value) {
        return value < 10 ? "0" + value : "" + value;
    }

    function formatTimestamp(value) {
        if (value === undefined || value === null || value === "") {
            return "-";
        }

        var text = ("" + value).trim();
        var epochSeconds = Number(text);
        if (!isNaN(epochSeconds) && epochSeconds > 0) {
            var date = new Date(epochSeconds * 1000);
            return date.getFullYear() + "-"
                    + pad2(date.getMonth() + 1) + "-"
                    + pad2(date.getDate()) + " "
                    + pad2(date.getHours()) + ":"
                    + pad2(date.getMinutes()) + ":"
                    + pad2(date.getSeconds());
        }

        if (text.indexOf("T") !== -1) {
            return text.replace("T", " ").replace("Z", "");
        }
        return text;
    }

    function selectAsset(row) {
        selectedRow = row;
        selectedAsset = assetModel.get(row);
        window.selectedAssetIdentity = selectedAsset.macAddress || "";
    }

    function rowMatches(row) {
        var query = searchQuery.trim().toLowerCase();
        if (query === "") {
            return true;
        }
        var asset = assetModel.get(row);
        var ip = primaryIp(asset).toLowerCase();
        var mac = (asset.macAddress || "").toLowerCase();
        var hostname = (asset.hostname || "").toLowerCase();
        var displayName = (asset.displayName || "").toLowerCase();
        var vendor = (asset.vendor || "").toLowerCase();
        var osHint = (asset.osHint || "").toLowerCase();
        var deviceType = (asset.deviceType || "").toLowerCase();
        var modelHint = (asset.modelHint || "").toLowerCase();
        return ip.indexOf(query) !== -1 || mac.indexOf(query) !== -1 || hostname.indexOf(query) !== -1
                || displayName.indexOf(query) !== -1 || vendor.indexOf(query) !== -1
                || osHint.indexOf(query) !== -1 || deviceType.indexOf(query) !== -1
                || modelHint.indexOf(query) !== -1;
    }

    function choosePcap() {
        var path = captureController.choosePcapFile();
        if (path !== "") {
            captureController.pcapPath = path;
        }
    }

    function chooseConfig() {
        var path = captureController.chooseConfigFile();
        if (path !== "") {
            captureController.configPath = path;
        }
    }

    function startCaptureFromMode(modeIndex) {
        captureController.packetFilter = fixedFilter;
        if (modeIndex === 0) {
            captureController.isLive = true;
            captureController.startLiveCapture();
        } else {
            captureController.isLive = false;
            captureController.startPcapAnalysis();
        }
    }

    function exportAssets() {
        var path = captureController.chooseExportFile(exportFormat);
        if (path === "") {
            exportStatus = "Export canceled";
            return;
        }
        exportStatus = assetModel.exportToFile(path, exportFormat)
                ? "Exported " + path
                : "Export failed";
    }

    function openPage(index) {
        currentPage = index;
    }

    function hasArgument(value) {
        for (var i = 0; i < Qt.application.arguments.length; ++i) {
            if (Qt.application.arguments[i] === value) {
                return true;
            }
        }
        return false;
    }

    Connections {
        target: assetModel
        function onAssetsChanged() {
            if (selectedRow >= 0 && selectedRow < assetModel.rowCount()) {
                selectedAsset = assetModel.get(selectedRow);
            } else if (assetModel.rowCount() > 0) {
                selectAsset(0);
            } else {
                selectedRow = -1;
                selectedAsset = ({});
            }
        }
    }

    Component.onCompleted: {
        captureController.packetFilter = fixedFilter;
        if (interfaceModel.rowCount() > 0 && captureController.interfaceName === "") {
            captureController.interfaceName = interfaceModel.systemNameAt(0);
        }
        if (assetModel.rowCount() > 0) {
            selectAsset(0);
        }
        if (hasArgument("--smoke-cycle")) {
            smokeCycleTimer.start();
        }
    }

    Timer {
        id: smokeCycleTimer
        interval: 25
        repeat: true
        onTriggered: {
            if (smokePageIndex >= pages.length) {
                stop();
                return;
            }
            currentPage = smokePageIndex;
            smokePageIndex += 1;
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 196
            Layout.fillHeight: true
            color: "#111827"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "PNAD"
                        color: "#ffffff"
                        font.pixelSize: 20
                        font.bold: true
                    }
                    Text {
                        text: "Passive Discovery"
                        color: "#b8c4d4"
                        font.pixelSize: 12
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#263244"
                }

                Repeater {
                    model: shell.pages
                    delegate: Button {
                        id: navButton
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        text: modelData.title
                        flat: true
                        checkable: true
                        checked: shell.currentPage === index
                        onClicked: shell.openPage(index)
                        contentItem: Text {
                            text: navButton.text
                            color: navButton.checked ? "#ffffff" : (navButton.hovered ? "#f8fafc" : "#cbd5e1")
                            font.pixelSize: 13
                            font.bold: navButton.checked
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                            elide: Text.ElideRight
                        }
                        background: Rectangle {
                            radius: 6
                            color: navButton.checked ? window.colorAccent
                                  : (navButton.hovered ? "#1f2a3a" : "transparent")
                            border.color: navButton.checked ? "#0d9488" : "transparent"
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#263244"
                }
                Text {
                    text: shell.runStateText()
                    color: shell.runStateColor()
                    font.pixelSize: 12
                    font.bold: true
                }
                Text {
                    text: captureController.statusText
                    color: "#b8c4d4"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                color: window.colorPanel
                border.color: window.colorBorder

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: shell.pages[shell.currentPage].title
                            color: window.colorTextMain
                            font.pixelSize: 19
                            font.bold: true
                        }
                        Text {
                            text: shell.currentSourceLabel()
                            color: window.colorTextMuted
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 116
                        Layout.preferredHeight: 32
                        radius: 6
                        color: captureController.isRunning ? "#dcfce7"
                              : (captureController.lastError === "" ? "#eef2f7" : "#fee2e2")
                        border.color: captureController.isRunning ? "#86efac"
                                      : (captureController.lastError === "" ? window.colorBorder : "#fecaca")
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 6
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: shell.runStateColor()
                            }
                            Text {
                                text: shell.runStateText()
                                color: shell.runStateColor()
                                font.pixelSize: 11
                                font.bold: true
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            StackLayout {
                id: pageStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: shell.currentPage

                DashboardPage { shell: shell; enabled: shell.currentPage === 0 }
                CapturePage { shell: shell; enabled: shell.currentPage === 1 }
                AssetsPage { shell: shell; enabled: shell.currentPage === 2 }
                EventsPage { shell: shell; enabled: shell.currentPage === 3 }
                SettingsPage { shell: shell; enabled: shell.currentPage === 4 }
            }
        }
    }
}
