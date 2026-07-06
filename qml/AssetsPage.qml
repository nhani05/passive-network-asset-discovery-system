import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: page
    property var shell

    RowLayout {
        anchors.fill: parent
        anchors.margins: 18
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
                        text: shell.searchQuery
                        color: window.colorTextMain
                        Layout.preferredWidth: 270
                        onTextChanged: shell.searchQuery = text
                    }
                }

                Text {
                    text: "Database: " + captureController.sqlitePath
                    color: window.colorTextMuted
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: assetModel
                    clip: true
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: shell.rowMatches(index) ? 42 : 0
                        visible: shell.rowMatches(index)
                        color: index === shell.selectedRow ? "#d9f0ed" : "transparent"
                        border.color: index === shell.selectedRow ? window.colorAccent : "transparent"

                        MouseArea {
                            anchors.fill: parent
                            onClicked: shell.selectAsset(index)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8
                            Text { text: ipAddresses && ipAddresses.length > 0 ? ipAddresses.join(", ") : "-"; color: window.colorTextMain; Layout.preferredWidth: 150; elide: Text.ElideRight }
                            Text { text: macAddress; color: window.colorTextMain; font.family: "monospace"; Layout.preferredWidth: 155; elide: Text.ElideRight }
                            Text { text: hostname; color: window.colorTextMain; Layout.preferredWidth: 130; elide: Text.ElideRight }
                            Text { text: shell.formatTimestamp(firstSeen); color: window.colorTextMuted; Layout.preferredWidth: 150; elide: Text.ElideRight }
                            Text { text: shell.formatTimestamp(lastSeen); color: window.colorTextMuted; Layout.preferredWidth: 150; elide: Text.ElideRight }
                            Text { text: discoverySources && discoverySources.length > 0 ? discoverySources.join(", ") : "-"; color: window.colorAccent; Layout.fillWidth: true; elide: Text.ElideRight }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 292
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
                        { label: "IP", value: shell.primaryIp(shell.selectedAsset) },
                        { label: "MAC", value: shell.selectedAsset.macAddress || "-" },
                        { label: "Hostname", value: shell.selectedAsset.hostname || "-" },
                        { label: "First Seen", value: shell.formatTimestamp(shell.selectedAsset.firstSeen) },
                        { label: "Last Seen", value: shell.formatTimestamp(shell.selectedAsset.lastSeen) },
                        { label: "Protocols", value: shell.protocols(shell.selectedAsset) }
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
                        onActivated: shell.exportFormat = currentText
                    }
                    Button {
                        text: "Export"
                        onClicked: shell.exportAssets()
                    }
                }
                Text {
                    text: shell.exportStatus
                    color: shell.exportStatus.indexOf("failed") !== -1 ? window.colorHigh : window.colorTextMuted
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }
    }
}
