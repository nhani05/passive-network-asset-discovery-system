import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: securityCenterView
    anchors.fill: parent

    property string searchQuery: ""
    property string severityFilter: "all"
    property int currentEventRow: -1
    property var currentEvent: ({})
    property string exportMessage: ""

    function extensionForFormat() {
        return captureController.outputFormat === "csv" ? "csv" : (captureController.outputFormat === "json" ? "json" : "txt");
    }

    function openEventRow(row) {
        currentEventRow = row;
        currentEvent = eventModel.get(row);
        eventDetailsPopup.open();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Security Events"
                font.pixelSize: 26
                font.bold: true
                color: window.colorTextMain
            }
            Item { Layout.fillWidth: true }
            TextField {
                id: searchField
                placeholderText: "Search severity, type, MAC, IP, hostname, protocol, interface, message"
                font.pixelSize: 13
                Layout.preferredWidth: 360
                color: window.colorTextMain
                placeholderTextColor: window.colorTextMuted
                background: Rectangle {
                    color: window.colorSidebar
                    border.color: searchField.activeFocus ? window.colorAccent : window.colorBorder
                    radius: 4
                }
                onTextChanged: securityCenterView.searchQuery = text.trim().toLowerCase()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Text { text: "Severity"; color: window.colorTextMuted; font.pixelSize: 13 }

            Repeater {
                model: [
                    { label: "All", value: "all", color: window.colorAccent },
                    { label: "High", value: "high", color: window.colorHigh },
                    { label: "Warning", value: "warning", color: window.colorWarn },
                    { label: "Info", value: "info", color: window.colorInfo }
                ]
                delegate: Button {
                    text: modelData.label
                    checkable: true
                    checked: securityCenterView.severityFilter === modelData.value
                    onClicked: securityCenterView.severityFilter = modelData.value
                    background: Rectangle {
                        color: parent.checked ? modelData.color : "transparent"
                        border.color: window.colorBorder
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked && modelData.value !== "warning" ? "#ffffff" : (parent.checked ? "#000000" : window.colorTextMain)
                        font.bold: parent.checked
                        padding: 6
                    }
                }
            }

            Item { Layout.fillWidth: true }
            Text {
                text: "Unresolved: " + eventModel.unresolvedCount + " | High: " + eventModel.highSeverityCount
                color: eventModel.highSeverityCount > 0 ? window.colorHigh : window.colorTextMuted
                font.pixelSize: 12
                font.bold: eventModel.highSeverityCount > 0
            }
        }

        Text {
            text: securityCenterView.exportMessage
            visible: text !== ""
            color: window.colorInfo
            font.pixelSize: 12
            Layout.fillWidth: true
            elide: Text.ElideMiddle
        }

        Rectangle {
            Layout.fillWidth: true
            height: 40
            color: window.colorSidebar
            border.color: window.colorBorder
            radius: 4
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Text { text: "Time"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 140; font.pixelSize: 12 }
                Text { text: "Severity"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 80; font.pixelSize: 12 }
                Text { text: "Type"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 180; font.pixelSize: 12 }
                Text { text: "Asset"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 230; font.pixelSize: 12 }
                Text { text: "Protocol / IF"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 135; font.pixelSize: 12 }
                Text { text: "Description"; font.bold: true; color: window.colorTextMain; Layout.fillWidth: true; font.pixelSize: 12 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            clip: true

            ListView {
                id: eventListView
                anchors.fill: parent
                model: eventModel
                delegate: Rectangle {
                    width: parent.width
                    height: visible ? 52 : 0
                    visible: eventModel.matchesFilter(index, securityCenterView.searchQuery, securityCenterView.severityFilter)
                    color: "transparent"

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: parent.color = "#334155"
                        onExited: parent.color = "transparent"
                        onClicked: securityCenterView.openEventRow(index)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10
                        Text { text: eventTime; color: window.colorTextMuted; font.pixelSize: 11; Layout.preferredWidth: 140; elide: Text.ElideRight }
                        Text {
                            text: severity.toUpperCase()
                            color: severity === "high" ? window.colorHigh : (severity === "warning" ? window.colorWarn : window.colorInfo)
                            font.pixelSize: 11
                            font.bold: true
                            Layout.preferredWidth: 80
                        }
                        Text { text: eventLabel; color: window.colorTextMain; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 180; elide: Text.ElideRight }
                        Text { text: assetLabel; color: window.colorAccent; font.pixelSize: 12; Layout.preferredWidth: 230; elide: Text.ElideRight }
                        Text { text: protocol + " / " + interface; color: window.colorTextMuted; font.pixelSize: 12; Layout.preferredWidth: 135; elide: Text.ElideRight }
                        Text { text: message; color: window.colorTextMain; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                    }

                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: window.colorBorder }
                }
            }

            Text {
                anchors.centerIn: parent
                text: eventModel.rowCount() === 0 ? "No security events recorded yet" : "No security events match the filters"
                color: window.colorTextMuted
                font.pixelSize: 14
                visible: eventModel.rowCount() === 0
            }
        }
    }

    Popup {
        id: eventDetailsPopup
        anchors.centerIn: parent
        width: 620
        height: 560
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: window.colorSidebar
            border.color: window.colorBorder
            radius: 8
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: securityCenterView.currentEvent.eventLabel || "Event Details"
                    color: window.colorTextMain
                    font.pixelSize: 18
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Text {
                    text: securityCenterView.currentEvent.severity ? securityCenterView.currentEvent.severity.toUpperCase() : ""
                    color: securityCenterView.currentEvent.severity === "high" ? window.colorHigh : window.colorWarn
                    font.pixelSize: 12
                    font.bold: true
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: window.colorBorder }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 14
                rowSpacing: 8
                Repeater {
                    model: [
                        { label: "Time", value: securityCenterView.currentEvent.eventTime || "-" },
                        { label: "Raw Type", value: securityCenterView.currentEvent.eventType || "-" },
                        { label: "Asset", value: securityCenterView.currentEvent.assetLabel || "-" },
                        { label: "Hostname", value: securityCenterView.currentEvent.hostname || "-" },
                        { label: "Old IP", value: securityCenterView.currentEvent.oldIp || "-" },
                        { label: "New IP", value: securityCenterView.currentEvent.newIp || "-" },
                        { label: "Old MAC", value: securityCenterView.currentEvent.oldMac || "-" },
                        { label: "New MAC", value: securityCenterView.currentEvent.newMac || "-" },
                        { label: "Protocol", value: securityCenterView.currentEvent.protocol || "-" },
                        { label: "Interface", value: securityCenterView.currentEvent.interface || "-" }
                    ]
                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text { text: modelData.label; color: window.colorTextMuted; font.pixelSize: 10; font.bold: true }
                        Text { text: modelData.value; color: window.colorTextMain; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                }
            }

            Text {
                text: securityCenterView.currentEvent.message || ""
                color: window.colorTextMain
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: window.colorBg
                border.color: window.colorBorder
                radius: 4
                clip: true
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true
                    Text {
                        width: parent.width - 20
                        text: securityCenterView.currentEvent.rawMetadata || "{}"
                        color: "#f472b6"
                        font.family: "monospace"
                        font.pixelSize: 11
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: evidencePath
                    text: "pnad-event-evidence." + securityCenterView.extensionForFormat()
                    color: window.colorTextMain
                    Layout.fillWidth: true
                    background: Rectangle { color: window.colorBg; border.color: window.colorBorder; radius: 4 }
                }
                Button {
                    text: "View Asset"
                    enabled: securityCenterView.currentEvent.macAddress !== "-" || securityCenterView.currentEvent.ipAddress !== "-"
                    onClicked: {
                        var query = securityCenterView.currentEvent.macAddress !== "-"
                            ? securityCenterView.currentEvent.macAddress
                            : securityCenterView.currentEvent.ipAddress;
                        eventDetailsPopup.close();
                        window.showAssetDetail(query);
                    }
                    background: Rectangle { color: enabled ? window.colorAccent : "transparent"; border.color: window.colorBorder; radius: 4 }
                    contentItem: Text { text: parent.text; color: enabled ? "#06201d" : window.colorTextMuted; font.bold: enabled; padding: 7 }
                }
                Button {
                    text: "Resolve"
                    enabled: false
                    ToolTip.visible: hovered
                    ToolTip.text: "Resolution persistence is not enabled in this passive MVP."
                    background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 4 }
                    contentItem: Text { text: parent.text; color: window.colorTextMuted; padding: 7 }
                }
                Button {
                    text: "Trust MAC"
                    enabled: false
                    ToolTip.visible: hovered
                    ToolTip.text: "Trusted-list persistence is not enabled in this passive MVP."
                    background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 4 }
                    contentItem: Text { text: parent.text; color: window.colorTextMuted; padding: 7 }
                }
                Button {
                    text: "Export Evidence"
                    onClicked: {
                        var ok = eventModel.exportEvidenceToFile(securityCenterView.currentEventRow, evidencePath.text, captureController.outputFormat);
                        securityCenterView.exportMessage = ok ? "Evidence exported to " + evidencePath.text : "Evidence export failed";
                    }
                    background: Rectangle { color: window.colorPanel; border.color: window.colorBorder; radius: 4 }
                    contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 7 }
                }
                Button {
                    text: "Close"
                    onClicked: eventDetailsPopup.close()
                    background: Rectangle { color: window.colorBorder; radius: 4 }
                    contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 7 }
                }
            }
        }
    }
}
