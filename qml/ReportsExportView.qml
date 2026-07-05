import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: reportsExportView
    anchors.fill: parent
    property string exportMessage: ""
    property bool exportError: false

    function extensionForFormat() {
        return captureController.outputFormat === "json" ? "json" : (captureController.outputFormat === "csv" ? "csv" : "txt");
    }

    function setExportResult(success, path) {
        exportError = !success;
        exportMessage = success ? ("Export written to " + path) : ("Export failed for " + path);
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 16
        Text { text: "Reports / Export"; color: window.colorTextMain; font.pixelSize: 26; font.bold: true }
        Text { text: "Prepare inventory, security event, PCAP analysis, and capture session reports from the current local database."; color: window.colorTextMuted; font.pixelSize: 13 }
        Text { text: reportsExportView.exportMessage; visible: text !== ""; color: reportsExportView.exportError ? window.colorWarn : window.colorInfo; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideMiddle }
        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: 14
            rowSpacing: 14
            Repeater {
                model: ["Asset Inventory", "Security Events", "PCAP Analysis", "Capture Session Summary"]
                delegate: Rectangle {
                    Layout.fillWidth: true
                    height: 150
                    color: window.colorCard
                    border.color: window.colorBorder
                    radius: 6
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        Text { text: modelData; color: window.colorTextMain; font.pixelSize: 16; font.bold: true }
                        Text { text: "Export format: " + captureController.outputFormat; color: window.colorTextMuted; font.pixelSize: 12 }
                        TextField {
                            id: exportPath
                            text: modelData === "Asset Inventory"
                                  ? "pnad-assets." + reportsExportView.extensionForFormat()
                                  : (modelData === "Security Events"
                                     ? "pnad-events." + reportsExportView.extensionForFormat()
                                     : (modelData === "PCAP Analysis"
                                        ? "pnad-pcap-analysis." + reportsExportView.extensionForFormat()
                                        : "pnad-capture-session-summary." + reportsExportView.extensionForFormat()))
                            color: window.colorTextMain
                            Layout.fillWidth: true
                            background: Rectangle { color: window.colorBg; border.color: window.colorBorder; radius: 4 }
                        }
                        Item { Layout.fillHeight: true }
                        Button {
                            text: "Export"
                            onClicked: {
                                var ok = false;
                                if (modelData === "Asset Inventory") {
                                    ok = assetModel.exportToFile(exportPath.text, captureController.outputFormat);
                                } else if (modelData === "Security Events") {
                                    ok = eventModel.exportToFile(exportPath.text, captureController.outputFormat);
                                } else {
                                    ok = analysisSessionModel.exportSummaryToFile(exportPath.text, captureController.outputFormat);
                                }
                                reportsExportView.setExportResult(ok, exportPath.text);
                            }
                            background: Rectangle { color: window.colorPanel; border.color: window.colorBorder; radius: 4 }
                            contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 7 }
                        }
                    }
                }
            }
        }
    }
}
