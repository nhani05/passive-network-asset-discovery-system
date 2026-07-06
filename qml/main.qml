import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
    id: window
    visible: true
    width: 1180
    height: 760
    minimumWidth: 980
    minimumHeight: 640
    title: "Passive Network Asset Discovery"

    readonly property color colorBg: "#f5f7fb"
    readonly property color colorPanel: "#ffffff"
    readonly property color colorPanelAlt: "#eef3f8"
    readonly property color colorBorder: "#c9d3df"
    readonly property color colorTextMain: "#17202a"
    readonly property color colorTextMuted: "#687789"
    readonly property color colorAccent: "#0f766e"
    readonly property color colorHigh: "#b91c1c"
    readonly property color colorWarn: "#b45309"
    readonly property color colorInfo: "#15803d"

    property string selectedAssetIdentity: ""

    function hasArgument(value) {
        for (var i = 0; i < Qt.application.arguments.length; ++i) {
            if (Qt.application.arguments[i] === value) {
                return true;
            }
        }
        return false;
    }

    function reloadDataModels() {
        assetModel.reloadFromDatabase(captureController.sqlitePath);
    }

    background: Rectangle { color: window.colorBg }

    Connections {
        target: captureController
        function onCaptureFinished() {
            reloadDataModels();
        }
        function onSqlitePathChanged() {
            reloadDataModels();
        }
    }

    Timer {
        id: smokeTimer
        interval: 250
        repeat: false
        onTriggered: Qt.quit()
    }

    Component.onCompleted: {
        reloadDataModels();
        if (hasArgument("--smoke-first-run") && mainLoader.source.toString().indexOf("AppShell.qml") === -1) {
            console.error("FirstRunAssertionFailed: expected AppShell, got " + mainLoader.source);
            Qt.quit();
            return;
        }
        if (hasArgument("--smoke-cycle")) {
            smokeTimer.start();
        }
    }

    Loader {
        id: mainLoader
        anchors.fill: parent
        source: "AppShell.qml"
    }
}
