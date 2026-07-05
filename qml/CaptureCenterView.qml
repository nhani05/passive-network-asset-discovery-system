import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: captureCenter
    anchors.fill: parent

    property int modeIndex: captureController.isLive ? 0 : 1
    property string liveMessage: ""
    property string pcapMessage: ""
    property double startedAtMs: 0
    property int elapsedSeconds: 0
    property bool showTabs: true

    function setMode(index) {
        modeIndex = index;
        modeTabs.currentIndex = index;
        if (!captureController.isRunning) {
            captureController.isLive = index === 0;
        }
    }

    function selectedInterfaceInfo() {
        if (interfaceList.currentIndex < 0) {
            return {};
        }
        return interfaceModel.get(interfaceList.currentIndex);
    }

    function applySharedFields() {
        captureController.packetFilter = txtCaptureFilter.text.trim();
        captureController.captureBackend = cmbBackend.currentText;
    }

    function backendReady(selected) {
        if (!selected.systemName) {
            return false;
        }
        if (cmbBackend.currentText === "pcap") {
            return selected.pcapAvailable;
        }
        if (cmbBackend.currentText === "af-packet") {
            return selected.afPacketAvailable;
        }
        return selected.pcapAvailable || selected.afPacketAvailable;
    }

    function backendDiagnostic(selected) {
        if (cmbBackend.currentText === "pcap") {
            return selected.pcapDiagnostic || "The pcap backend is not available for this interface.";
        }
        if (cmbBackend.currentText === "af-packet") {
            return selected.afPacketDiagnostic || "The af-packet backend is not available for this interface.";
        }
        return "No supported capture backend is ready for this interface.";
    }

    function restoreRecentSources() {
        if (captureController.interfaceName === "") {
            var recentInterface = analysisSessionModel.latestSourceForMode("Live Capture");
            if (recentInterface !== "" && interfaceModel.findBySystemName(recentInterface) >= 0) {
                captureController.interfaceName = recentInterface;
                interfaceList.currentIndex = interfaceModel.findBySystemName(recentInterface);
            }
        }
        if (captureController.pcapPath === "") {
            var recentPcap = analysisSessionModel.latestSourceForMode("PCAP Analysis");
            if (recentPcap !== "") {
                captureController.pcapPath = recentPcap;
                txtPcapPath.text = recentPcap;
            }
        }
    }

    Timer {
        interval: 1000
        running: captureController.isRunning
        repeat: true
        onTriggered: {
            if (captureCenter.startedAtMs > 0) {
                captureCenter.elapsedSeconds = Math.floor((Date.now() - captureCenter.startedAtMs) / 1000);
            }
        }
    }

    Component.onCompleted: restoreRecentSources()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: captureCenter.modeIndex === 0 ? "Live Capture" : "PCAP Analysis"
                color: window.colorTextMain
                font.pixelSize: 26
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text {
                text: captureController.isRunning ? "Running" : "Ready"
                color: captureController.isRunning ? window.colorInfo : window.colorTextMuted
                font.pixelSize: 13
                font.bold: true
            }
        }

        TabBar {
            id: modeTabs
            currentIndex: captureCenter.modeIndex
            Layout.fillWidth: true
            Layout.preferredHeight: captureCenter.showTabs ? implicitHeight : 0
            visible: captureCenter.showTabs
            onCurrentIndexChanged: captureCenter.setMode(currentIndex)
            TabButton { text: "Live Capture" }
            TabButton { text: "PCAP Analysis" }
        }

        Rectangle {
            Layout.fillWidth: true
            height: captureController.isRunning ? 54 : 0
            visible: captureController.isRunning
            color: window.colorCard
            border.color: window.colorBorder
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: window.colorInfo
                }
                Text {
                    text: (captureController.isLive ? "Live Capture" : "PCAP Analysis")
                          + " running for " + captureCenter.elapsedSeconds + "s"
                    color: window.colorTextMain
                    font.pixelSize: 13
                    font.bold: true
                }
                Text {
                    text: captureController.isLive ? captureController.interfaceName : captureController.pcapPath
                    color: window.colorTextMuted
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
                Button {
                    text: captureController.isLive ? "Stop" : "Cancel"
                    onClicked: captureController.stopCapture()
                    background: Rectangle { color: window.colorHigh; radius: 4 }
                    contentItem: Text { text: parent.text; color: "#ffffff"; font.bold: true; padding: 8 }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: captureCenter.modeIndex

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Network interfaces"; color: window.colorTextMain; font.pixelSize: 18; font.bold: true }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "Refresh"
                        onClicked: interfaceModel.refresh()
                        background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 4 }
                        contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 8 }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 290
                    color: "transparent"
                    border.color: window.colorBorder
                    border.width: 1
                    radius: 6
                    clip: true

                    ListView {
                        id: interfaceList
                        anchors.fill: parent
                        model: interfaceModel
                        currentIndex: interfaceModel.findBySystemName(captureController.interfaceName)
                        delegate: Rectangle {
                            width: interfaceList.width
                            height: 78
                            color: ListView.isCurrentItem ? window.colorPanel : "transparent"
                            border.color: ListView.isCurrentItem ? window.colorAccent : window.colorBorder
                            border.width: ListView.isCurrentItem ? 1 : 0

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    interfaceList.currentIndex = index;
                                    captureController.interfaceName = systemName;
                                    captureCenter.liveMessage = "";
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 14
                                Rectangle {
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: captureAllowed ? window.colorInfo : (isUp ? window.colorWarn : window.colorHigh)
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Text { text: systemName; color: window.colorTextMain; font.pixelSize: 14; font.bold: true }
                                    Text {
                                        text: (addresses.length > 0 ? addresses.join(", ") : "No address") + "  |  " + macAddress
                                        color: window.colorTextMuted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: (isUp && isRunning ? "Up and running" : "Not running")
                                              + (isLoopback ? "  |  loopback" : "")
                                              + (isVirtual ? "  |  virtual" : "")
                                              + "  |  backend "
                                              + ((pcapAvailable || afPacketAvailable)
                                                 ? ((pcapAvailable ? "pcap" : "") + (pcapAvailable && afPacketAvailable ? ", " : "") + (afPacketAvailable ? "af-packet" : ""))
                                                 : "unavailable")
                                              + (permissionDiagnostic ? "  |  " + permissionDiagnostic : "")
                                        color: captureAllowed ? window.colorTextMuted : window.colorWarn
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: "Capture mode"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                        ComboBox {
                            model: ["Infinite"]
                            currentIndex: 0
                            enabled: false
                            Layout.fillWidth: true
                            background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 }
                            contentItem: Text { text: "Infinite"; color: window.colorTextMain; verticalAlignment: Text.AlignVCenter; leftPadding: 10 }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: "Stop conditions"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                        TextField {
                            text: "No-packet timeout and asset limit unavailable"
                            enabled: false
                            Layout.fillWidth: true
                            color: window.colorTextMuted
                            background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: "Capture filter"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                        TextField {
                            id: txtCaptureFilter
                            text: captureController.packetFilter
                            Layout.fillWidth: true
                            color: window.colorTextMain
                            placeholderTextColor: window.colorTextMuted
                            placeholderText: "arp or udp port 67 or udp port 68"
                            background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: "Backend policy"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                        ComboBox {
                            id: cmbBackend
                            model: ["auto", "pcap", "af-packet"]
                            currentIndex: model.indexOf(captureController.captureBackend)
                            Layout.fillWidth: true
                            background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 }
                            contentItem: Text { text: cmbBackend.currentText; color: window.colorTextMain; verticalAlignment: Text.AlignVCenter; leftPadding: 10 }
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 10
                    rowSpacing: 10
                    Repeater {
                        model: [
                            { label: "Runtime", value: captureController.isRunning && captureController.isLive ? captureCenter.elapsedSeconds + "s" : "Stopped" },
                            { label: "Packets captured", value: "Unavailable" },
                            { label: "Assets found", value: assetModel.rowCount() },
                            { label: "Dropped packets", value: "Unavailable" }
                        ]
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 64
                            color: window.colorCard
                            border.color: window.colorBorder
                            radius: 5
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 3
                                Text { text: modelData.label; color: window.colorTextMuted; font.pixelSize: 10; font.bold: true }
                                Text { text: modelData.value; color: modelData.value === "Unavailable" ? window.colorWarn : window.colorTextMain; font.pixelSize: 13; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                            }
                        }
                    }
                }

                Text {
                    text: "Status log: " + captureController.runtimeLogPath
                    color: window.colorTextMuted
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                }

                Text {
                    text: captureCenter.liveMessage
                    visible: text !== ""
                    color: window.colorWarn
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: captureController.isRunning && captureController.isLive ? "Stop Live Capture" : "Start Live Capture"
                        onClicked: {
                            if (captureController.isRunning && captureController.isLive) {
                                captureController.stopCapture();
                                return;
                            }
                            var selected = captureCenter.selectedInterfaceInfo();
                            if (!selected.systemName) {
                                captureCenter.liveMessage = "Choose a network interface before starting Live Capture.";
                                return;
                            }
                            if (!selected.captureAllowed) {
                                captureCenter.liveMessage = selected.permissionDiagnostic || "This interface is not ready for live capture.";
                                return;
                            }
                            if (!captureCenter.backendReady(selected)) {
                                captureCenter.liveMessage = captureCenter.backendDiagnostic(selected);
                                return;
                            }
                            captureController.interfaceName = selected.systemName;
                            captureCenter.applySharedFields();
                            captureCenter.startedAtMs = Date.now();
                            captureCenter.elapsedSeconds = 0;
                            captureController.startLiveCapture();
                        }
                        background: Rectangle { color: captureController.isRunning && captureController.isLive ? window.colorHigh : window.colorInfo; radius: 5 }
                        contentItem: Text { text: parent.text; color: "#ffffff"; font.bold: true; padding: 10; horizontalAlignment: Text.AlignHCenter }
                    }
                    Button {
                        text: "Restart"
                        enabled: captureController.isRunning && captureController.isLive
                        onClicked: {
                            captureController.stopCapture();
                            captureCenter.applySharedFields();
                            captureCenter.startedAtMs = Date.now();
                            captureCenter.elapsedSeconds = 0;
                            captureController.startLiveCapture();
                        }
                        background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 5 }
                        contentItem: Text { text: parent.text; color: enabled ? window.colorTextMain : window.colorTextMuted; padding: 10 }
                    }
                    Item { Layout.fillWidth: true }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16

                Text { text: "Analyze a PCAP file"; color: window.colorTextMain; font.pixelSize: 18; font.bold: true }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 168
                    color: window.colorCard
                    border.color: dropArea.containsDrag ? window.colorAccent : window.colorBorder
                    border.width: 1
                    radius: 6

                    DropArea {
                        id: dropArea
                        anchors.fill: parent
                        onDropped: {
                            if (drop.hasUrls && drop.urls.length > 0) {
                                var path = drop.urls[0].toString();
                                if (path.indexOf("file://") === 0) {
                                    path = path.substring(7);
                                }
                                txtPcapPath.text = decodeURIComponent(path);
                                captureCenter.pcapMessage = "";
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12
                        Text {
                            text: "Drop a PCAP or PCAPNG file here"
                            color: window.colorTextMain
                            font.pixelSize: 16
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: "Use packet captures from Wireshark, tcpdump, sensors, or previous investigations."
                            color: window.colorTextMuted
                            font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                id: txtPcapPath
                                text: captureController.pcapPath
                                placeholderText: "/path/to/capture.pcap"
                                color: window.colorTextMain
                                placeholderTextColor: window.colorTextMuted
                                Layout.fillWidth: true
                                background: Rectangle { color: window.colorBg; border.color: window.colorBorder; radius: 4 }
                            }
                            Button {
                                text: "Browse"
                                onClicked: {
                                    var path = captureController.choosePcapFile();
                                    if (path !== "") {
                                        txtPcapPath.text = path;
                                        captureCenter.pcapMessage = "";
                                    }
                                }
                                background: Rectangle { color: window.colorPanel; border.color: window.colorBorder; radius: 4 }
                                contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 8 }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: captureController.pcapPath === "" ? 0 : 42
                    visible: captureController.pcapPath !== ""
                    color: "transparent"
                    border.color: window.colorBorder
                    radius: 4
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        Text { text: "Recent PCAP"; color: window.colorTextMuted; font.pixelSize: 12; font.bold: true }
                        Text { text: captureController.pcapPath; color: window.colorTextMain; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideMiddle }
                        Button {
                            text: "Use"
                            onClicked: txtPcapPath.text = captureController.pcapPath
                            background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 4 }
                            contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 5 }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Capture filter"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                    TextField {
                        id: txtPcapFilter
                        text: captureController.packetFilter
                        placeholderText: "Optional BPF capture filter"
                        color: window.colorTextMain
                        placeholderTextColor: window.colorTextMuted
                        Layout.fillWidth: true
                        background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 5
                    columnSpacing: 10
                    rowSpacing: 10
                    Repeater {
                        model: [
                            { label: "Packets parsed", value: "Unavailable" },
                            { label: "ARP packets", value: "Unavailable" },
                            { label: "DHCP packets", value: "Unavailable" },
                            { label: "Assets found", value: analysisSessionModel.latestAssetCount },
                            { label: "Events found", value: analysisSessionModel.latestEventCount }
                        ]
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 64
                            color: window.colorCard
                            border.color: window.colorBorder
                            radius: 5
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 3
                                Text { text: modelData.label; color: window.colorTextMuted; font.pixelSize: 10; font.bold: true }
                                Text { text: modelData.value; color: modelData.value === "Unavailable" ? window.colorWarn : window.colorTextMain; font.pixelSize: 13; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                            }
                        }
                    }
                }

                Text {
                    text: captureCenter.pcapMessage
                    visible: text !== ""
                    color: window.colorWarn
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: captureController.isRunning && !captureController.isLive ? "Cancel Analysis" : "Analyze PCAP"
                        onClicked: {
                            if (captureController.isRunning && !captureController.isLive) {
                                captureController.stopCapture();
                                return;
                            }
                            if (txtPcapPath.text.trim() === "") {
                                captureCenter.pcapMessage = "Choose a PCAP file before starting analysis.";
                                return;
                            }
                            captureController.pcapPath = txtPcapPath.text.trim();
                            captureController.packetFilter = txtPcapFilter.text.trim();
                            if (!captureController.validatePcapAnalysisRequest()) {
                                captureCenter.pcapMessage = captureController.validationError;
                                return;
                            }
                            captureCenter.startedAtMs = Date.now();
                            captureCenter.elapsedSeconds = 0;
                            captureController.startPcapAnalysis();
                        }
                        background: Rectangle { color: captureController.isRunning && !captureController.isLive ? window.colorHigh : window.colorInfo; radius: 5 }
                        contentItem: Text { text: parent.text; color: "#ffffff"; font.bold: true; padding: 10; horizontalAlignment: Text.AlignHCenter }
                    }
                    Button {
                        text: "View Results"
                        enabled: assetModel.rowCount() > 0 || eventModel.rowCount() > 0
                        onClicked: window.setNavSource("AssetInventoryView.qml")
                        background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 5 }
                        contentItem: Text { text: parent.text; color: enabled ? window.colorTextMain : window.colorTextMuted; padding: 10 }
                    }
                    Button {
                        text: "Export Report"
                        enabled: analysisSessionModel.rowCountForQml() > 0
                        onClicked: window.setNavSource("ReportsExportView.qml")
                        background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 5 }
                        contentItem: Text { text: parent.text; color: enabled ? window.colorTextMain : window.colorTextMuted; padding: 10 }
                    }
                    Text {
                        text: "Compare mode: existing/new/changed is derived from inventory and event data after analysis."
                        color: window.colorTextMuted
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }
    }
}
