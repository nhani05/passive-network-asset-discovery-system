import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: assetDetailView
    anchors.fill: parent

    property var asset: ({})
    property var timeline: []

    function safeMetadata(value) {
        if (!value || value === "") {
            return "{}";
        }
        try {
            return JSON.stringify(JSON.parse(value), null, 4);
        } catch (e) {
            return value;
        }
    }

    function reloadAsset() {
        asset = assetModel.assetForIdentity(window.selectedAssetIdentity);
        timeline = assetModel.timelineForAsset(window.selectedAssetIdentity);
    }

    Component.onCompleted: reloadAsset()

    Connections {
        target: assetModel
        function onAssetsChanged() {
            assetDetailView.reloadAsset();
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 28
        clip: true

        ColumnLayout {
            width: parent.width - 24
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Asset Detail"
                    color: window.colorTextMain
                    font.pixelSize: 26
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Back to Inventory"
                    onClicked: window.showAssetsForQuery(window.selectedAssetIdentity)
                    background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 4 }
                    contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 8 }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: Object.keys(asset).length === 0 ? 72 : 0
                visible: Object.keys(asset).length === 0
                color: window.colorCard
                border.color: window.colorBorder
                radius: 6
                Text {
                    anchors.centerIn: parent
                    text: "No asset found for " + window.selectedAssetIdentity
                    color: window.colorTextMuted
                    font.pixelSize: 13
                }
            }

            GridLayout {
                visible: Object.keys(asset).length > 0
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 12
                rowSpacing: 12

                Repeater {
                    model: [
                        { label: "IP Address", value: asset.ipAddresses && asset.ipAddresses.length > 0 ? asset.ipAddresses.join(", ") : "No IP" },
                        { label: "MAC Address", value: asset.macAddress || "-" },
                        { label: "Hostname", value: asset.hostname && asset.hostname !== "-" ? asset.hostname : "Unknown" },
                        { label: "Vendor", value: asset.vendor || "Unknown" },
                        { label: "Status", value: asset.status || "Unknown" },
                        { label: "Risk", value: asset.risk || "Normal" },
                        { label: "First Seen", value: asset.firstSeen || "-" },
                        { label: "Last Seen", value: asset.lastSeen || "-" }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        height: 78
                        color: window.colorCard
                        border.color: window.colorBorder
                        radius: 6
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 5
                            Text { text: modelData.label; color: window.colorTextMuted; font.pixelSize: 10; font.bold: true }
                            Text {
                                text: modelData.value
                                color: modelData.label === "Risk" && modelData.value === "High"
                                       ? window.colorHigh
                                       : (modelData.label === "Risk" && modelData.value === "Suspicious"
                                          ? window.colorWarn
                                          : window.colorTextMain)
                                font.pixelSize: 13
                                font.bold: modelData.label === "Risk" || modelData.label === "Status"
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: Object.keys(asset).length > 0
                Layout.fillWidth: true
                height: 60
                color: "transparent"
                border.color: window.colorBorder
                radius: 6
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10
                    Text { text: "Detected by"; color: window.colorTextMuted; font.pixelSize: 11; font.bold: true }
                    Text {
                        text: asset.sourceSummary || "Unknown"
                        color: window.colorAccent
                        font.pixelSize: 13
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }

            Text {
                visible: Object.keys(asset).length > 0
                text: "Timeline"
                color: window.colorTextMain
                font.pixelSize: 18
                font.bold: true
            }

            Rectangle {
                visible: Object.keys(asset).length > 0
                Layout.fillWidth: true
                height: Math.max(190, timeline.length * 46 + 20)
                color: window.colorCard
                border.color: window.colorBorder
                radius: 6
                clip: true

                ListView {
                    anchors.fill: parent
                    anchors.margins: 10
                    interactive: height < contentHeight
                    model: timeline
                    delegate: Rectangle {
                        width: parent.width
                        height: 46
                        color: "transparent"
                        RowLayout {
                            anchors.fill: parent
                            spacing: 12
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: modelData.severity === "high"
                                       ? window.colorHigh
                                       : (modelData.severity === "warning" ? window.colorWarn : window.colorInfo)
                            }
                            Text {
                                text: modelData.time
                                color: window.colorTextMuted
                                font.pixelSize: 11
                                Layout.preferredWidth: 145
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.type
                                color: window.colorTextMain
                                font.pixelSize: 12
                                font.bold: true
                                Layout.preferredWidth: 150
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.message
                                color: window.colorTextMain
                                font.pixelSize: 12
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: window.colorBorder }
                    }
                }
            }

            Text {
                visible: Object.keys(asset).length > 0
                text: "Observed Metadata"
                color: window.colorTextMain
                font.pixelSize: 18
                font.bold: true
            }

            Rectangle {
                visible: Object.keys(asset).length > 0
                Layout.fillWidth: true
                height: 220
                color: window.colorBg
                border.color: window.colorBorder
                radius: 6
                clip: true
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true
                    Text {
                        width: parent.width - 20
                        text: assetDetailView.safeMetadata(asset.rawObservedMetadata)
                        color: "#67e8f9"
                        font.family: "monospace"
                        font.pixelSize: 11
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
        }
    }
}
