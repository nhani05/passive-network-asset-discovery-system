import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: dashboardView
    anchors.fill: parent

    function isEmptyProductState() {
        return assetModel.rowCount() === 0 && analysisSessionModel.completedSessionCount === 0;
    }

    function metricColor(label, value) {
        if (label === "High Risk Events" && value > 0) return window.colorHigh;
        if (label === "Capture") return captureController.isRunning ? window.colorInfo : window.colorTextMain;
        return window.colorAccent;
    }

    ScrollView {
        anchors.fill: parent
        anchors.topMargin: 24
        anchors.bottomMargin: 24
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            // Use the scroll view's viewport width, leave room for vertical scrollbar
            width: dashboardView.width - 56
            spacing: 20

            // ── Header ────────────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "Dashboard"
                    font.pixelSize: 24
                    font.bold: true
                    color: window.colorTextMain
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Refresh"
                    onClicked: window.reloadDataModels()
                    background: Rectangle {
                        color: "transparent"
                        border.color: window.colorBorder
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: window.colorTextMain
                        padding: 8
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // ── Empty state call-to-action ─────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                height: dashboardView.isEmptyProductState() ? ctaBanner.implicitHeight + 32 : 0
                visible: dashboardView.isEmptyProductState()
                color: window.colorCard
                border.color: window.colorBorder
                radius: 6

                RowLayout {
                    id: ctaBanner
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: "Start discovery"
                            color: window.colorTextMain
                            font.pixelSize: 15
                            font.bold: true
                        }
                        Text {
                            text: "Run Live Capture or analyze a PCAP to populate inventory, events, reports, and health context."
                            color: window.colorTextMuted
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                    Button {
                        text: "Live Capture"
                        onClicked: window.showLiveCapture()
                        background: Rectangle { color: window.colorInfo; radius: 4 }
                        contentItem: Text { text: parent.text; color: "#06201d"; font.bold: true; padding: 9; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                    Button {
                        text: "PCAP Analysis"
                        onClicked: window.showPcapAnalysis()
                        background: Rectangle { color: window.colorAccent; radius: 4 }
                        contentItem: Text { text: parent.text; color: "#06201d"; font.bold: true; padding: 9; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }
            }

            // ── Primary metrics (2 rows × 4 cols) ─────────────────────────
            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 10
                rowSpacing: 10

                Repeater {
                    model: [
                        { label: "Total Assets",     value: assetModel.rowCount(),            detail: "Inventory records",          isNum: true  },
                        { label: "Active Assets",    value: assetModel.activeAssetCount,      detail: "Seen in active window",      isNum: true  },
                        { label: "New Today",        value: assetModel.newAssetsTodayCount,   detail: "First seen today",           isNum: true  },
                        { label: "High Risk Events", value: eventModel.highSeverityCount,     detail: "Needs review",               isNum: true  },
                        { label: "Capture",          value: captureController.isRunning ? "RUNNING" : "STOPPED",
                                                     detail: captureController.statusText,    isNum: false },
                        { label: "Events",           value: eventModel.rowCount(),            detail: "Total anomalies / events",   isNum: true  },
                        { label: "Latest Session",   value: analysisSessionModel.latestSessionLabel,
                                                     detail: "Assets " + analysisSessionModel.latestAssetCount + " | Events " + analysisSessionModel.latestEventCount,
                                                                                              isNum: false },
                        { label: "Newest Asset",     value: assetModel.newestAssetLabel,     detail: "Latest first-seen record",   isNum: false }
                    ]

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        // Tall enough for label + big number + detail, never clips
                        height: 100
                        color: window.colorCard
                        border.color: window.colorBorder
                        radius: 6

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 4

                            Text {
                                text: modelData.label
                                color: window.colorTextMuted
                                font.pixelSize: 10
                                font.bold: true
                                font.letterSpacing: 0.5
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: modelData.value
                                color: dashboardView.metricColor(modelData.label, modelData.value)
                                // Numbers get a large display size; strings (labels) get a smaller size
                                font.pixelSize: modelData.isNum ? 30 : 14
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                // Prevent the huge number from pushing the detail text off-screen
                                Layout.maximumHeight: modelData.isNum ? 40 : 36
                            }

                            Item { Layout.fillHeight: true }

                            Text {
                                text: modelData.detail
                                color: window.colorTextMuted
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            // ── Runtime counters (3 cols, muted / informational) ──────────
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 10
                rowSpacing: 0

                Repeater {
                    model: [
                        { label: "Packets/sec",     value: "Unavailable", detail: "Runtime counter not exposed"          },
                        { label: "Parsed packets",  value: "Unavailable", detail: "Use session counts where available"   },
                        { label: "Dropped packets", value: "Unavailable", detail: "Not estimated"                        }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        height: 68
                        color: "transparent"
                        border.color: window.colorBorder
                        radius: 6

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 3

                            Text {
                                text: modelData.label
                                color: window.colorTextMuted
                                font.pixelSize: 10
                                font.bold: true
                            }
                            Text {
                                text: modelData.value
                                color: window.colorWarn
                                font.pixelSize: 13
                                font.bold: true
                            }
                            Text {
                                text: modelData.detail
                                color: window.colorTextMuted
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            // ── Recent Events ─────────────────────────────────────────────
            Text {
                text: "Recent Events"
                font.pixelSize: 16
                font.bold: true
                color: window.colorTextMain
            }

            Rectangle {
                Layout.fillWidth: true
                // Show up to 8 rows (48px each) + top/bottom margin; min 80px for empty state
                height: eventModel.rowCount() === 0 ? 80 : Math.min(eventModel.rowCount(), 8) * 48 + 20
                color: window.colorCard
                border.color: window.colorBorder
                radius: 6
                clip: true

                ListView {
                    id: recentEvents
                    anchors.fill: parent
                    anchors.margins: 10
                    model: eventModel
                    interactive: false   // outer ScrollView handles scrolling
                    delegate: Item {
                        width: parent ? parent.width : 0
                        height: index < 8 ? 48 : 0
                        visible: index < 8

                        Rectangle {
                            id: rowBg
                            anchors.fill: parent
                            color: "transparent"
                            radius: 4
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: rowBg.color = "#1e293b"
                            onExited:  rowBg.color = "transparent"
                            onClicked: {
                                var query = macAddress !== "-" ? macAddress : ipAddress;
                                if (query && query !== "-") {
                                    window.showAssetDetail(query);
                                } else {
                                    window.showEventDetail(index);
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            // Severity dot
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: severity === "high"    ? window.colorHigh :
                                       severity === "warning" ? window.colorWarn : window.colorInfo
                            }

                            // Timestamp
                            Text {
                                text: eventTime
                                color: window.colorTextMuted
                                font.pixelSize: 11
                                Layout.preferredWidth: 130
                                elide: Text.ElideRight
                            }

                            // Event label
                            Text {
                                text: eventLabel
                                color: severity === "high" ? window.colorHigh : window.colorTextMain
                                font.pixelSize: 12
                                font.bold: true
                                Layout.preferredWidth: 150
                                elide: Text.ElideRight
                            }

                            // Asset identifier
                            Text {
                                text: assetLabel
                                color: window.colorAccent
                                font.pixelSize: 12
                                Layout.preferredWidth: 160
                                elide: Text.ElideRight
                            }

                            // Message — takes remaining space
                            Text {
                                text: message
                                color: window.colorTextMuted
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }

                        // Row divider
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: window.colorBorder
                            visible: index < Math.min(eventModel.rowCount(), 8) - 1
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "No security events recorded yet"
                    color: window.colorTextMuted
                    font.pixelSize: 13
                    visible: eventModel.rowCount() === 0
                }
            }

            // Bottom spacer so the last card isn't flush against the window edge
            Item { height: 8 }
        }
    }
}
