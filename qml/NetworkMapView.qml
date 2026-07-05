import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: networkMapView
    anchors.fill: parent
    property var selectedAsset: null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 16
        Text { text: "Network Map"; color: window.colorTextMain; font.pixelSize: 26; font.bold: true }
        Text { text: "Assets grouped by stable network identity from the local discovery database."; color: window.colorTextMuted; font.pixelSize: 13 }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            border.color: window.colorBorder
            radius: 6
            clip: true
            ListView {
                anchors.fill: parent
                anchors.margins: 8
                model: assetModel
                delegate: Rectangle {
                    width: parent.width
                    height: 60
                    color: networkMapView.selectedAsset && networkMapView.selectedAsset.macAddress === macAddress ? window.colorPanel : "transparent"
                    border.color: window.colorBorder

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            networkMapView.selectedAsset = {
                                macAddress: macAddress,
                                ipAddresses: ipAddresses,
                                hostname: hostname,
                                discoverySources: discoverySources,
                                group: assetModel.networkGroupForRow(index)
                            };
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        Rectangle { width: 10; height: 10; radius: 5; color: window.colorInfo }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: assetModel.networkGroupForRow(index); color: window.colorAccent; font.pixelSize: 12; font.bold: true }
                            Text { text: ipAddresses.length > 0 ? ipAddresses[0] : "Unassigned address"; color: window.colorTextMain; font.pixelSize: 14; font.bold: true }
                            Text { text: macAddress + "  |  " + discoverySources.join(", "); color: window.colorTextMuted; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                        Text { text: hostname === "-" ? "" : hostname; color: window.colorTextMuted; font.pixelSize: 12 }
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                text: "No assets available for the map"
                color: window.colorTextMuted
                visible: assetModel.rowCount() === 0
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: networkMapView.selectedAsset ? 86 : 0
            visible: networkMapView.selectedAsset !== null
            color: window.colorCard
            border.color: window.colorBorder
            radius: 6
            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 14
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: networkMapView.selectedAsset ? networkMapView.selectedAsset.macAddress : ""; color: window.colorTextMain; font.pixelSize: 14; font.bold: true }
                    Text { text: networkMapView.selectedAsset ? networkMapView.selectedAsset.group + " | " + networkMapView.selectedAsset.ipAddresses.join(", ") : ""; color: window.colorTextMuted; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                }
                Button {
                    text: "Open Asset Detail"
                    onClicked: window.showAssetDetail(networkMapView.selectedAsset.macAddress)
                    background: Rectangle { color: window.colorPanel; border.color: window.colorBorder; radius: 4 }
                    contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 7 }
                }
            }
        }
    }
}
