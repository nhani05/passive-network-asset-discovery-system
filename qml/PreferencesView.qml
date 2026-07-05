import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: preferencesView
    anchors.fill: parent
    property string message: ""
    property bool messageIsError: false

    function splitCsv(value) {
        var result = [];
        var parts = value.split(",");
        for (var i = 0; i < parts.length; ++i) {
            var item = parts[i].trim();
            if (item !== "") result.push(item);
        }
        return result;
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 28
        clip: true

        ColumnLayout {
            width: parent.width - 24
            spacing: 18

            Text { text: "Settings"; color: window.colorTextMain; font.pixelSize: 26; font.bold: true }
            Text {
                text: "Saved defaults for future Live Capture, PCAP Analysis, reporting, and diagnostics."
                color: window.colorTextMuted
                font.pixelSize: 13
                Layout.fillWidth: true
            }

            Text {
                text: preferencesView.message
                visible: text !== ""
                color: preferencesView.messageIsError ? window.colorWarn : window.colorInfo
                font.pixelSize: 12
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 18
                rowSpacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Default interface"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                    TextField { id: txtInterface; text: captureController.interfaceName; placeholderText: "wlan0, eth0, enp3s0"; color: window.colorTextMain; placeholderTextColor: window.colorTextMuted; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Default capture mode"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                    ComboBox { id: cmbMode; model: ["Live Capture", "PCAP Analysis"]; currentIndex: captureController.isLive ? 0 : 1; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 }; contentItem: Text { text: cmbMode.currentText; color: window.colorTextMain; verticalAlignment: Text.AlignVCenter; leftPadding: 10 } }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Capture filter"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                    TextField { id: txtFilter; text: captureController.packetFilter; color: window.colorTextMain; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Backend policy"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                    ComboBox { id: cmbBackend; model: ["auto", "pcap", "af-packet"]; currentIndex: model.indexOf(captureController.captureBackend); Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 }; contentItem: Text { text: cmbBackend.currentText; color: window.colorTextMain; verticalAlignment: Text.AlignVCenter; leftPadding: 10 } }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Local database"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                    TextField { id: txtDatabase; text: captureController.sqlitePath; color: window.colorTextMain; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Export format"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                    ComboBox { id: cmbExport; model: ["json", "table", "csv"]; currentIndex: model.indexOf(captureController.outputFormat); Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 }; contentItem: Text { text: cmbExport.currentText; color: window.colorTextMain; verticalAlignment: Text.AlignVCenter; leftPadding: 10 } }
                }
            }

            Text { text: "Detection rules"; color: window.colorTextMain; font.pixelSize: 16; font.bold: true }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 18
                rowSpacing: 14

                ColumnLayout { Layout.fillWidth: true; Text { text: "Duplicate event suppression"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }; TextField { id: txtRate; text: captureController.eventRateLimitSeconds.toString(); color: window.colorTextMain; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } } }
                ColumnLayout { Layout.fillWidth: true; Text { text: "IP change detection window"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }; TextField { id: txtWindow; text: captureController.flipFlopWindowSeconds.toString(); color: window.colorTextMain; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } } }
                ColumnLayout { Layout.fillWidth: true; Text { text: "Reappearance threshold"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }; TextField { id: txtReappearance; text: captureController.reappearanceThresholdSeconds.toString(); color: window.colorTextMain; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } } }
                ColumnLayout { Layout.fillWidth: true; Text { text: "Local networks"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }; TextField { id: txtLocal; text: captureController.localNetworks.join(", "); color: window.colorTextMain; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } } }
                ColumnLayout { Layout.fillWidth: true; Text { text: "Ignored networks"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }; TextField { id: txtIgnored; text: captureController.ignoredNetworks.join(", "); color: window.colorTextMain; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } } }
                ColumnLayout { Layout.fillWidth: true; Text { text: "No-packet timeout"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }; TextField { text: "Unavailable in current engine"; enabled: false; color: window.colorTextMuted; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } } }
                ColumnLayout { Layout.fillWidth: true; Text { text: "Max asset limit"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }; TextField { text: "Unavailable in current engine"; enabled: false; color: window.colorTextMuted; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } } }
                ColumnLayout { Layout.fillWidth: true; Text { text: "Trusted MAC list"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }; TextField { text: "Disabled in passive MVP"; enabled: false; color: window.colorTextMuted; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } } }
                ColumnLayout { Layout.fillWidth: true; Text { text: "Log level"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }; TextField { text: "Default"; enabled: false; color: window.colorTextMuted; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } } }
            }

            Button {
                id: advancedToggle
                text: advancedPanel.visible ? "Hide advanced engine settings" : "Show advanced engine settings"
                onClicked: advancedPanel.visible = !advancedPanel.visible
                background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 4 }
                contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 8 }
            }

            GridLayout {
                id: advancedPanel
                visible: false
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 18
                rowSpacing: 14

                ColumnLayout { Layout.fillWidth: true; Text { text: "Event buffer capacity"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }; TextField { id: txtQueue; text: captureController.eventQueueCapacity.toString(); color: window.colorTextMain; Layout.fillWidth: true; background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 } } }
            }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "Reset Defaults"
                    onClicked: {
                        captureController.loadDefaults();
                        txtInterface.text = captureController.interfaceName;
                        cmbMode.currentIndex = captureController.isLive ? 0 : 1;
                        txtFilter.text = captureController.packetFilter;
                        cmbBackend.currentIndex = cmbBackend.model.indexOf(captureController.captureBackend);
                        txtDatabase.text = captureController.sqlitePath;
                        cmbExport.currentIndex = cmbExport.model.indexOf(captureController.outputFormat);
                        txtRate.text = captureController.eventRateLimitSeconds.toString();
                        txtQueue.text = captureController.eventQueueCapacity.toString();
                        txtWindow.text = captureController.flipFlopWindowSeconds.toString();
                        txtReappearance.text = captureController.reappearanceThresholdSeconds.toString();
                        txtLocal.text = captureController.localNetworks.join(", ");
                        txtIgnored.text = captureController.ignoredNetworks.join(", ");
                        preferencesView.message = "";
                    }
                    background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 4 }
                    contentItem: Text { text: parent.text; color: window.colorTextMuted; padding: 9 }
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Save Preferences"
                    onClicked: {
                        var previousDatabase = captureController.sqlitePath;
                        captureController.interfaceName = txtInterface.text.trim();
                        captureController.isLive = cmbMode.currentIndex === 0;
                        captureController.packetFilter = txtFilter.text.trim();
                        captureController.captureBackend = cmbBackend.currentText;
                        captureController.sqlitePath = txtDatabase.text.trim();
                        captureController.outputFormat = cmbExport.currentText;
                        var rate = parseInt(txtRate.text); if (!isNaN(rate)) captureController.eventRateLimitSeconds = rate;
                        var queue = parseInt(txtQueue.text); if (!isNaN(queue)) captureController.eventQueueCapacity = queue;
                        var win = parseInt(txtWindow.text); if (!isNaN(win)) captureController.flipFlopWindowSeconds = win;
                        var reappear = parseInt(txtReappearance.text); if (!isNaN(reappear)) captureController.reappearanceThresholdSeconds = reappear;
                        captureController.localNetworks = splitCsv(txtLocal.text);
                        captureController.ignoredNetworks = splitCsv(txtIgnored.text);
                        if (!captureController.validatePreferences()) {
                            if (captureController.validationError.indexOf("local database") !== -1) {
                                captureController.sqlitePath = previousDatabase;
                                txtDatabase.text = previousDatabase;
                            }
                            preferencesView.message = captureController.validationError;
                            preferencesView.messageIsError = true;
                            return;
                        }
                        captureController.saveSettingsToDb();
                        preferencesView.message = "Preferences saved.";
                        preferencesView.messageIsError = false;
                    }
                    background: Rectangle { color: window.colorAccent; radius: 4 }
                    contentItem: Text { text: parent.text; color: "#06201d"; font.bold: true; padding: 10 }
                }
            }
        }
    }
}
