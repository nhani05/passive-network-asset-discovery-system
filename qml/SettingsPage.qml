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

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: preferencesContent.implicitHeight + 28
            Layout.minimumHeight: preferencesContent.implicitHeight + 28
            color: window.colorPanel
            border.color: window.colorBorder
            radius: 6

            ColumnLayout {
                id: preferencesContent
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text {
                    text: "Preferences"
                    color: window.colorTextMain
                    font.pixelSize: 15
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "SQLite path"; color: window.colorTextMuted; Layout.preferredWidth: 110 }
                    TextField { text: captureController.sqlitePath; color: window.colorTextMain; Layout.fillWidth: true; onEditingFinished: captureController.sqlitePath = text }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Config path"; color: window.colorTextMuted; Layout.preferredWidth: 110 }
                    TextField { text: captureController.configPath; color: window.colorTextMain; Layout.fillWidth: true; onEditingFinished: captureController.configPath = text }
                    Button { text: "Browse"; onClicked: shell.chooseConfig() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Profile"; color: window.colorTextMuted; Layout.preferredWidth: 110 }
                    TextField { text: captureController.profileName; color: window.colorTextMain; Layout.fillWidth: true; onEditingFinished: captureController.profileName = text }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Protocols"; color: window.colorTextMuted; Layout.preferredWidth: 110 }
                    Text {
                        text: shell.supportedProtocols
                        color: window.colorTextMain
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Email alerts"; color: window.colorTextMuted; Layout.preferredWidth: 110 }
                    Text {
                        text: captureController.emailAlertsEnabled ? "Configured in .env" : "Disabled in .env"
                        color: captureController.emailAlertsEnabled ? window.colorAccent : window.colorTextMuted
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Recipients"; color: window.colorTextMuted; Layout.preferredWidth: 110 }
                    TextField {
                        text: captureController.emailRecipients
                        placeholderText: "admin@example.com, noc@example.com"
                        color: window.colorTextMain
                        Layout.fillWidth: true
                        onEditingFinished: captureController.emailRecipients = text
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: actionsContent.implicitHeight + 28
            Layout.minimumHeight: actionsContent.implicitHeight + 28
            color: window.colorPanel
            border.color: window.colorBorder
            radius: 6

            ColumnLayout {
                id: actionsContent
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "Settings Actions"
                    color: window.colorTextMain
                    font.pixelSize: 15
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Button { text: "Load Defaults"; onClicked: captureController.loadDefaults() }
                    Button { text: "Save Settings"; onClicked: captureController.saveSettingsToDb() }
                    Button { text: "Load Settings"; onClicked: captureController.loadSettingsFromDb() }
                    Button { text: "Validate"; onClicked: captureController.validatePreferences() }
                }

                Text {
                    text: captureController.validationError === "" ? "Validation: -" : "Validation: " + captureController.validationError
                    color: captureController.validationError === "" ? window.colorTextMuted : window.colorHigh
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Text {
                    text: captureController.lastError === "" ? "Last error: -" : "Last error: " + captureController.lastError
                    color: captureController.lastError === "" ? window.colorTextMuted : window.colorHigh
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
