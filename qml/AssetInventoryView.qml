import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: assetInventoryView
    anchors.fill: parent

    property string searchQuery: ""
    property string statusFilter: "all"
    property string riskFilter: "all"
    property string sourceFilter: "all"

    function setSearchQuery(query) {
        searchQuery = query.toLowerCase();
        searchField.text = query;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Asset Inventory"
                font.pixelSize: 26
                font.bold: true
                color: window.colorTextMain
            }
            Item { Layout.fillWidth: true }
            TextField {
                id: searchField
                placeholderText: "Search IP, MAC, hostname, vendor, role, source"
                font.pixelSize: 13
                Layout.preferredWidth: 340
                color: window.colorTextMain
                placeholderTextColor: window.colorTextMuted
                background: Rectangle {
                    color: window.colorSidebar
                    border.color: searchField.activeFocus ? window.colorAccent : window.colorBorder
                    radius: 4
                }
                onTextChanged: assetInventoryView.searchQuery = text.trim().toLowerCase()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                id: cmbStatus
                model: ["all", "Active", "Recently Seen", "Offline", "Unknown"]
                currentIndex: 0
                Layout.preferredWidth: 150
                onCurrentTextChanged: assetInventoryView.statusFilter = currentText
                background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 }
                contentItem: Text { text: "Status: " + cmbStatus.currentText; color: window.colorTextMain; verticalAlignment: Text.AlignVCenter; leftPadding: 8; elide: Text.ElideRight }
            }
            ComboBox {
                id: cmbRisk
                model: ["all", "Normal", "Suspicious", "High"]
                currentIndex: 0
                Layout.preferredWidth: 145
                onCurrentTextChanged: assetInventoryView.riskFilter = currentText
                background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 }
                contentItem: Text { text: "Risk: " + cmbRisk.currentText; color: window.colorTextMain; verticalAlignment: Text.AlignVCenter; leftPadding: 8; elide: Text.ElideRight }
            }
            ComboBox {
                id: cmbSource
                model: ["all", "arp", "dhcp", "dns", "pcap", "live"]
                currentIndex: 0
                Layout.preferredWidth: 145
                onCurrentTextChanged: assetInventoryView.sourceFilter = currentText
                background: Rectangle { color: window.colorCard; border.color: window.colorBorder; radius: 4 }
                contentItem: Text { text: "Source: " + cmbSource.currentText; color: window.colorTextMain; verticalAlignment: Text.AlignVCenter; leftPadding: 8; elide: Text.ElideRight }
            }
            Button {
                text: "Sort Last Seen"
                onClicked: assetModel.sortByLastSeenDescending()
                background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 4 }
                contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 8 }
            }
            Button {
                text: "Sort MAC"
                onClicked: assetModel.sortByMacAddress()
                background: Rectangle { color: "transparent"; border.color: window.colorBorder; radius: 4 }
                contentItem: Text { text: parent.text; color: window.colorTextMain; padding: 8 }
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "Active filters: " + [assetInventoryView.statusFilter, assetInventoryView.riskFilter, assetInventoryView.sourceFilter].join(" / ")
                color: window.colorTextMuted
                font.pixelSize: 12
            }
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
                Text { text: "IP Address"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 150; font.pixelSize: 12 }
                Text { text: "MAC Address"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 160; font.pixelSize: 12 }
                Text { text: "Hostname"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 130; font.pixelSize: 12 }
                Text { text: "Vendor"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 150; font.pixelSize: 12 }
                Text { text: "First Seen"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 140; font.pixelSize: 12 }
                Text { text: "Last Seen"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 140; font.pixelSize: 12 }
                Text { text: "Status"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 115; font.pixelSize: 12 }
                Text { text: "Risk"; font.bold: true; color: window.colorTextMain; Layout.preferredWidth: 105; font.pixelSize: 12 }
                Text { text: "Source"; font.bold: true; color: window.colorTextMain; Layout.fillWidth: true; font.pixelSize: 12 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            clip: true

            ListView {
                id: assetListView
                anchors.fill: parent
                model: assetModel

                delegate: Rectangle {
                    width: parent.width
                    height: visible ? 52 : 0
                    visible: assetModel.matchesFilters(
                        index,
                        assetInventoryView.searchQuery,
                        assetInventoryView.statusFilter,
                        assetInventoryView.riskFilter,
                        assetInventoryView.sourceFilter)
                    color: "transparent"

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: parent.color = "#334155"
                        onExited: parent.color = "transparent"
                        onClicked: window.showAssetDetail(macAddress)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        Text { text: ipAddresses.length > 0 ? ipAddresses.join(", ") : "-"; color: window.colorTextMain; Layout.preferredWidth: 150; font.pixelSize: 12; elide: Text.ElideRight }
                        Text { text: macAddress; color: window.colorTextMain; font.family: "monospace"; Layout.preferredWidth: 160; font.pixelSize: 12; elide: Text.ElideRight }
                        Text { text: hostname; color: window.colorTextMain; Layout.preferredWidth: 130; font.pixelSize: 12; elide: Text.ElideRight }
                        Text { text: vendor; color: window.colorTextMain; Layout.preferredWidth: 150; font.pixelSize: 12; elide: Text.ElideRight }
                        Text { text: firstSeen; color: window.colorTextMuted; Layout.preferredWidth: 140; font.pixelSize: 11; elide: Text.ElideRight }
                        Text { text: lastSeen; color: window.colorTextMuted; Layout.preferredWidth: 140; font.pixelSize: 11; elide: Text.ElideRight }
                        Text { text: status; color: status === "Active" ? window.colorInfo : window.colorTextMuted; Layout.preferredWidth: 115; font.pixelSize: 12; font.bold: status === "Active"; elide: Text.ElideRight }
                        Text {
                            text: risk
                            color: risk === "High" ? window.colorHigh : (risk === "Suspicious" ? window.colorWarn : window.colorTextMain)
                            Layout.preferredWidth: 105
                            font.pixelSize: 12
                            font.bold: risk !== "Normal"
                            elide: Text.ElideRight
                        }
                        Text { text: sourceSummary; color: window.colorAccent; Layout.fillWidth: true; font.pixelSize: 12; elide: Text.ElideRight }
                    }

                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: window.colorBorder }
                }
            }

            Text {
                anchors.centerIn: parent
                text: assetModel.rowCount() === 0 ? "No discovered assets yet" : "No assets match the current filters"
                color: window.colorTextMuted
                font.pixelSize: 14
                visible: assetModel.rowCount() === 0
            }
        }
    }
}
