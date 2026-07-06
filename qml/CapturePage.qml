import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: page
    property var shell
    property int captureTabIndex: captureController.isLive ? 0 : 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: captureSourceContent.implicitHeight + 28
            Layout.minimumHeight: captureSourceContent.implicitHeight + 28
            color: window.colorPanel
            border.color: window.colorBorder
            radius: 6

            ColumnLayout {
                id: captureSourceContent
                anchors.fill: parent
                anchors.margins: 14
                spacing: 14

                Text {
                    text: "Capture Source"
                    color: window.colorTextMain
                    font.pixelSize: 15
                    font.bold: true
                }

                TabBar {
                    id: captureTabs
                    Layout.fillWidth: true
                    currentIndex: page.captureTabIndex
                    enabled: !captureController.isRunning
                    onCurrentIndexChanged: {
                        page.captureTabIndex = currentIndex;
                        captureController.isLive = currentIndex === 0;
                    }
                    TabButton { text: "Live Capture" }
                    TabButton { text: "PCAP File" }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: page.captureTabIndex === 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Interface"; color: window.colorTextMuted; Layout.preferredWidth: 92 }
                        ComboBox {
                            id: interfaceCombo
                            model: interfaceModel
                            textRole: "systemName"
                            enabled: !captureController.isRunning
                            Layout.preferredWidth: 260
                            onActivated: captureController.interfaceName = interfaceModel.systemNameAt(currentIndex)
                            Component.onCompleted: currentIndex = interfaceModel.findBySystemName(captureController.interfaceName)
                        }
                        Button {
                            text: "Refresh"
                            enabled: !captureController.isRunning
                            onClicked: {
                                interfaceModel.refresh();
                                interfaceCombo.currentIndex = interfaceModel.findBySystemName(captureController.interfaceName);
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Protocols"; color: window.colorTextMuted; Layout.preferredWidth: 92 }
                        Text {
                            text: shell.supportedProtocols
                            color: window.colorTextMain
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Button {
                            text: "Start Live"
                            enabled: !captureController.isRunning
                            onClicked: shell.startCaptureFromMode(0)
                        }
                        Button {
                            text: "Stop"
                            enabled: captureController.isRunning
                            onClicked: captureController.stopCapture()
                        }
                        Text {
                            text: captureController.statusText
                            color: window.colorTextMuted
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: page.captureTabIndex === 1

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "PCAP file"; color: window.colorTextMuted; Layout.preferredWidth: 92 }
                        TextField {
                            text: captureController.pcapPath
                            enabled: !captureController.isRunning
                            placeholderText: "PCAP file"
                            color: window.colorTextMain
                            Layout.fillWidth: true
                            onEditingFinished: captureController.pcapPath = text
                        }
                        Button {
                            text: "Browse"
                            enabled: !captureController.isRunning
                            onClicked: shell.choosePcap()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Protocols"; color: window.colorTextMuted; Layout.preferredWidth: 92 }
                        Text {
                            text: shell.supportedProtocols
                            color: window.colorTextMain
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Button {
                            text: "Analyze PCAP"
                            enabled: !captureController.isRunning
                            onClicked: shell.startCaptureFromMode(1)
                        }
                        Button {
                            text: "Stop"
                            enabled: captureController.isRunning
                            onClicked: captureController.stopCapture()
                        }
                        Text {
                            text: captureController.statusText
                            color: window.colorTextMuted
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: captureFeedbackContent.implicitHeight + 28
            Layout.minimumHeight: captureFeedbackContent.implicitHeight + 28
            color: window.colorPanel
            border.color: window.colorBorder
            radius: 6

            ColumnLayout {
                id: captureFeedbackContent
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                Text {
                    text: "Capture Feedback"
                    color: window.colorTextMain
                    font.pixelSize: 15
                    font.bold: true
                }
                Text { text: "Last error: " + (captureController.lastError === "" ? "-" : captureController.lastError); color: captureController.lastError === "" ? window.colorTextMuted : window.colorHigh; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: "Validation: " + (captureController.validationError === "" ? "-" : captureController.validationError); color: captureController.validationError === "" ? window.colorTextMuted : window.colorHigh; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: "Recent failure: " + (captureController.recentFailureSummary === "" ? "-" : captureController.recentFailureSummary); color: captureController.recentFailureSummary === "" ? window.colorTextMuted : window.colorWarn; Layout.fillWidth: true; elide: Text.ElideRight }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
