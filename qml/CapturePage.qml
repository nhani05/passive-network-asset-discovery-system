import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: page
    property var shell
    property int captureTabIndex: captureController.isLive ? 0 : 1
    property bool userSelectedInterface: false

    function syncInterfaceSelection() {
        var preferredName = interfaceModel.preferredSystemName(captureController.interfaceName, page.userSelectedInterface)
        if (preferredName !== "" && preferredName !== captureController.interfaceName) {
            captureController.interfaceName = preferredName
        }
        interfaceCombo.currentIndex = interfaceModel.findBySystemName(captureController.interfaceName)
    }

    function selectedInterfaceDiagnostic() {
        if (interfaceCombo.currentIndex < 0) {
            return "No capture interface available"
        }
        var row = interfaceModel.get(interfaceCombo.currentIndex)
        if (row.readinessDiagnostic !== undefined && row.readinessDiagnostic !== "") {
            return row.readinessDiagnostic
        }
        return row.permissionDiagnostic === "" ? row.readiness : row.permissionDiagnostic
    }

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
                    TabButton { text: "PCAP/PCAPNG" }
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
                            textRole: "displayLabel"
                            enabled: !captureController.isRunning
                            Layout.preferredWidth: 360
                            onActivated: {
                                page.userSelectedInterface = true
                                captureController.interfaceName = interfaceModel.systemNameAt(currentIndex)
                            }
                            Component.onCompleted: page.syncInterfaceSelection()
                        }
                        Button {
                            text: "Refresh"
                            enabled: !captureController.isRunning
                            onClicked: {
                                interfaceModel.refresh();
                                page.syncInterfaceSelection();
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Status"; color: window.colorTextMuted; Layout.preferredWidth: 92 }
                        Text {
                            text: page.selectedInterfaceDiagnostic()
                            color: interfaceCombo.currentIndex >= 0 && interfaceModel.get(interfaceCombo.currentIndex).captureAllowed ? window.colorTextMain : window.colorWarn
                            Layout.fillWidth: true
                            elide: Text.ElideRight
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
                        Text { text: "Capture file"; color: window.colorTextMuted; Layout.preferredWidth: 92 }
                        TextField {
                            text: captureController.pcapPath
                            enabled: !captureController.isRunning
                            placeholderText: "PCAP/PCAPNG file"
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
                        Text { text: "File types"; color: window.colorTextMuted; Layout.preferredWidth: 92 }
                        Text {
                            text: shell.supportedCaptureFiles
                            color: window.colorTextMain
                            Layout.fillWidth: true
                            elide: Text.ElideRight
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
