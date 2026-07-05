import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    anchors.fill: parent

    CaptureCenterView {
        id: captureView
        anchors.fill: parent
        showTabs: false
        Component.onCompleted: setMode(0)
    }
}
