import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: coreView
    anchors.fill: parent

    property int selectedRow: -1
    property var selectedAsset: ({})
    property string searchQuery: ""
    property string exportFormat: "json"
    readonly property string fixedFilter: "arp or (udp and (port 67 or port 68))"

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
        return ip.indexOf(query) !== -1 || mac.indexOf(query) !== -1 || hostname.indexOf(query) !== -1;
    }

    function choosePcap() {
        var path = captureController.choosePcapFile();
        if (path !== "") {
            captureController.pcapPath = path;
        }
    }

    function startCoreCapture() {
        captureController.packetFilter = fixedFilter;
        if (modeCombo.currentIndex === 0) {
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
            exportStatus.text = "Export canceled";
            return;
        }
        exportStatus.text = assetModel.exportToFile(path, exportFormat)
                ? "Exported " + path
                : "Export failed";
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
            interfaceCombo.currentIndex = 0;
            captureController.interfaceName = interfaceModel.systemNameAt(0);
        } else {
            interfaceCombo.currentIndex = interfaceModel.findBySystemName(captureController.interfaceName);
        }
        if (assetModel.rowCount() > 0) {
            selectAsset(0);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 104
            color: window.colorPanel
            border.color: window.colorBorder
            radius: 6

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "Passive Network Asset Discovery"
                        color: window.colorTextMain
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: captureController.isRunning ? window.colorInfo
                              : (captureController.lastError === "" ? window.colorTextMuted : window.colorHigh)
                    }
                    Text {
                        text: captureController.isRunning ? "RUNNING"
                              : (captureController.lastError === "" ? "STOPPED" : "ERROR")
                        color: captureController.isRunning ? window.colorInfo
                              : (captureController.lastError === "" ? window.colorTextMuted : window.colorHigh)
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Text {
                        text: captureController.statusText
                        color: window.colorTextMuted
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.maximumWidth: 260
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ComboBox {
                        id: modeCombo
                        model: ["Live", "PCAP"]
                        currentIndex: captureController.isLive ? 0 : 1
                        Layout.preferredWidth: 96
                        onActivated: captureController.isLive = currentIndex === 0
                    }
                    ComboBox {
                        id: interfaceCombo
                        model: interfaceModel
                        textRole: "systemName"
                        enabled: modeCombo.currentIndex === 0 && !captureController.isRunning
                        Layout.preferredWidth: 160
                        onActivated: captureController.interfaceName = interfaceModel.systemNameAt(currentIndex)
                    }
                    Button {
                        text: "Refresh"
                        enabled: !captureController.isRunning
                        onClicked: {
                            interfaceModel.refresh();
                            interfaceCombo.currentIndex = interfaceModel.findBySystemName(captureController.interfaceName);
                        }
                    }
                    TextField {
                        text: captureController.pcapPath
                        enabled: modeCombo.currentIndex === 1 && !captureController.isRunning
                        placeholderText: "PCAP file"
                        color: window.colorTextMain
                        Layout.fillWidth: true
                        onEditingFinished: captureController.pcapPath = text
                    }
                    Button {
                        text: "PCAP..."
                        enabled: modeCombo.currentIndex === 1 && !captureController.isRunning
                        onClicked: choosePcap()
                    }
                    Button {
                        text: "Start"
                        enabled: !captureController.isRunning
                        onClicked: startCoreCapture()
                    }
                    Button {
                        text: "Stop"
                        enabled: captureController.isRunning
                        onClicked: captureController.stopCapture()
                    }
                    Button {
                        text: "Clear Log"
                        onClicked: logModel.clear()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: captureController.lastError === "" ? 0 : 34
            visible: captureController.lastError !== ""
            color: "#fee2e2"
            border.color: "#fecaca"
            radius: 4
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                Text {
                    text: captureController.lastError
                    color: window.colorHigh
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Button {
                    text: "Dismiss"
                    onClicked: captureController.clearError()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: window.colorPanel
                border.color: window.colorBorder
                radius: 6

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Assets"
                            color: window.colorTextMain
                            font.pixelSize: 15
                            font.bold: true
                        }
                        Text {
                            text: assetModel.rowCount() + " found"
                            color: window.colorTextMuted
                            font.pixelSize: 12
                        }
                        Item { Layout.fillWidth: true }
                        TextField {
                            placeholderText: "Search IP, MAC, hostname"
                            text: coreView.searchQuery
                            color: window.colorTextMain
                            Layout.preferredWidth: 250
                            onTextChanged: coreView.searchQuery = text
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 34
                        color: window.colorPanelAlt
                        border.color: window.colorBorder
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8
                            Text { text: "IP"; color: window.colorTextMain; font.bold: true; Layout.preferredWidth: 150 }
                            Text { text: "MAC"; color: window.colorTextMain; font.bold: true; Layout.preferredWidth: 155 }
                            Text { text: "Hostname"; color: window.colorTextMain; font.bold: true; Layout.preferredWidth: 130 }
                            Text { text: "First Seen"; color: window.colorTextMain; font.bold: true; Layout.preferredWidth: 135 }
                            Text { text: "Last Seen"; color: window.colorTextMain; font.bold: true; Layout.preferredWidth: 135 }
                            Text { text: "Protocols"; color: window.colorTextMain; font.bold: true; Layout.fillWidth: true }
                        }
                    }

                    ListView {
                        id: assetList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: assetModel
                        clip: true
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: coreView.rowMatches(index) ? 42 : 0
                            visible: coreView.rowMatches(index)
                            color: index === coreView.selectedRow ? "#d9f0ed" : "transparent"
                            border.color: index === coreView.selectedRow ? window.colorAccent : "transparent"

                            MouseArea {
                                anchors.fill: parent
                                onClicked: coreView.selectAsset(index)
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8
                                Text { text: ipAddresses && ipAddresses.length > 0 ? ipAddresses.join(", ") : "-"; color: window.colorTextMain; Layout.preferredWidth: 150; elide: Text.ElideRight }
                                Text { text: macAddress; color: window.colorTextMain; font.family: "monospace"; Layout.preferredWidth: 155; elide: Text.ElideRight }
                                Text { text: hostname; color: window.colorTextMain; Layout.preferredWidth: 130; elide: Text.ElideRight }
                                Text { text: firstSeen; color: window.colorTextMuted; Layout.preferredWidth: 135; elide: Text.ElideRight }
                                Text { text: lastSeen; color: window.colorTextMuted; Layout.preferredWidth: 135; elide: Text.ElideRight }
                                Text { text: discoverySources && discoverySources.length > 0 ? discoverySources.join(", ") : "-"; color: window.colorAccent; Layout.fillWidth: true; elide: Text.ElideRight }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                color: window.colorPanel
                border.color: window.colorBorder
                radius: 6

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text {
                        text: "Asset Detail"
                        color: window.colorTextMain
                        font.pixelSize: 15
                        font.bold: true
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: window.colorBorder }

                    Repeater {
                        model: [
                            { label: "IP", value: coreView.primaryIp(coreView.selectedAsset) },
                            { label: "MAC", value: coreView.selectedAsset.macAddress || "-" },
                            { label: "Hostname", value: coreView.selectedAsset.hostname || "-" },
                            { label: "First Seen", value: coreView.selectedAsset.firstSeen || "-" },
                            { label: "Last Seen", value: coreView.selectedAsset.lastSeen || "-" },
                            { label: "Protocols", value: coreView.protocols(coreView.selectedAsset) }
                        ]
                        delegate: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: modelData.label
                                color: window.colorTextMuted
                                font.pixelSize: 11
                                font.bold: true
                            }
                            Text {
                                text: modelData.value
                                color: window.colorTextMain
                                font.pixelSize: 12
                                wrapMode: Text.WrapAnywhere
                                Layout.fillWidth: true
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        ComboBox {
                            model: ["json", "csv"]
                            Layout.fillWidth: true
                            onActivated: coreView.exportFormat = currentText
                        }
                        Button {
                            text: "Export"
                            onClicked: coreView.exportAssets()
                        }
                    }
                    Text {
                        id: exportStatus
                        text: ""
                        color: text.indexOf("failed") !== -1 ? window.colorHigh : window.colorTextMuted
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            color: window.colorPanel
            border.color: window.colorBorder
            radius: 6

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "New Assets"
                        color: window.colorTextMain
                        font.pixelSize: 14
                        font.bold: true
                    }
                    Text {
                        text: logModel.rowCount() + " log entries"
                        color: window.colorTextMuted
                        font.pixelSize: 11
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Filter: " + coreView.fixedFilter
                        color: window.colorTextMuted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.maximumWidth: 420
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: logModel
                    clip: true
                    delegate: RowLayout {
                        width: ListView.view.width
                        height: 24
                        spacing: 8
                        Text { text: timestamp; color: window.colorTextMuted; font.pixelSize: 11; Layout.preferredWidth: 170; elide: Text.ElideRight }
                        Text { text: "NEW"; color: window.colorAccent; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 42 }
                        Text { text: message; color: window.colorTextMain; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                }
            }
        }
    }
}
