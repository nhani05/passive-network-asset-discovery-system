import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: page
    property var shell

    Rectangle {
        anchors.fill: parent
        anchors.margins: 18
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
                    text: "Events"
                    color: window.colorTextMain
                    font.pixelSize: 15
                    font.bold: true
                }
                Text {
                    text: logModel.rowCount() + " log entries"
                    color: window.colorTextMuted
                    font.pixelSize: 12
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Clear Log"
                    onClicked: logModel.clear()
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
                    Text { text: "Timestamp"; color: window.colorTextMain; font.bold: true; Layout.preferredWidth: 170 }
                    Text { text: "Severity"; color: window.colorTextMain; font.bold: true; Layout.preferredWidth: 90 }
                    Text { text: "Source"; color: window.colorTextMain; font.bold: true; Layout.preferredWidth: 130 }
                    Text { text: "Message"; color: window.colorTextMain; font.bold: true; Layout.fillWidth: true }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: eventList
                    anchors.fill: parent
                    model: logModel
                    clip: true
                    delegate: RowLayout {
                        width: ListView.view.width
                        height: 28
                        spacing: 8
                        Text { text: shell.formatTimestamp(timestamp); color: window.colorTextMuted; font.pixelSize: 11; Layout.preferredWidth: 170; elide: Text.ElideRight }
                        Text { text: severity; color: severity === "error" ? window.colorHigh : window.colorAccent; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 90; elide: Text.ElideRight }
                        Text { text: source; color: window.colorTextMuted; font.pixelSize: 12; Layout.preferredWidth: 130; elide: Text.ElideRight }
                        Text { text: message; color: window.colorTextMain; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "No event entries"
                    visible: logModel.rowCount() === 0
                    color: window.colorTextMuted
                    font.pixelSize: 13
                }
            }
        }
    }
}
