import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: page
    property var shell

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Repeater {
                model: [
                    { label: "Capture", value: shell.runStateText(), accent: shell.runStateColor() },
                    { label: "Assets", value: assetModel.rowCount() + " found", accent: window.colorAccent },
                    { label: "Events", value: logModel.rowCount() + " entries", accent: window.colorWarn }
                ]
                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 92
                    color: window.colorPanel
                    border.color: window.colorBorder
                    radius: 6

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4
                        Text {
                            text: modelData.label
                            color: window.colorTextMuted
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Text {
                            text: modelData.value
                            color: modelData.accent
                            font.pixelSize: 20
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 164
            color: window.colorPanel
            border.color: window.colorBorder
            radius: 6

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text {
                    text: "Current Session"
                    color: window.colorTextMain
                    font.pixelSize: 15
                    font.bold: true
                }
                Text { text: "Status: " + captureController.statusText; color: window.colorTextMain; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: "Source: " + shell.currentSourceLabel(); color: window.colorTextMain; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: "Supported protocols: " + shell.supportedProtocols; color: window.colorTextMuted; Layout.fillWidth: true; elide: Text.ElideRight }
                Text {
                    text: captureController.lastError === "" ? "Error: none" : "Error: " + captureController.lastError
                    color: captureController.lastError === "" ? window.colorTextMuted : window.colorHigh
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Button { text: "Capture"; onClicked: shell.openPage(1) }
            Button { text: "Assets"; onClicked: shell.openPage(2) }
            Button { text: "Events"; onClicked: shell.openPage(3) }
        }

        Item { Layout.fillHeight: true }
    }
}
