import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    anchors.fill: parent

    function refreshHealth() {
        healthDiagnosticsModel.refresh(
            captureController.sqlitePath,
            captureController.interfaceName,
            captureController.captureBackend,
            captureController.lastError,
            captureController.statusText,
            captureController.recentFailureSummary,
            captureController.runtimeLogPath);
    }

    Timer {
        interval: 3000
        running: true
        repeat: true
        onTriggered: refreshHealth()
    }

    Component.onCompleted: refreshHealth()

    ScrollView {
        anchors.fill: parent
        anchors.margins: 28
        clip: true

        ColumnLayout {
            width: parent.width - 24
            spacing: 14
            Text { text: "System Health"; color: window.colorTextMain; font.pixelSize: 26; font.bold: true }
            Text {
                text: "Runtime readiness for database, capture engine, parser, queues, dropped counters, resources, and desktop environment."
                color: window.colorTextMuted
                font.pixelSize: 13
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: healthDiagnosticsModel
                delegate: Rectangle {
                    Layout.fillWidth: true
                    height: 88
                    color: window.colorCard
                    border.color: window.colorBorder
                    radius: 6
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 12
                        Rectangle {
                            width: 12
                            height: 12
                            radius: 6
                            color: state === "OK" ? window.colorInfo : (state === "Error" ? window.colorHigh : window.colorWarn)
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text { text: category + " - " + state; color: window.colorTextMain; font.pixelSize: 15; font.bold: true }
                            Text { text: detail; color: window.colorTextMuted; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: remediation; visible: remediation !== ""; color: window.colorTextMuted; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                        }
                    }
                }
            }
        }
    }
}
