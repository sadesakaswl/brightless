import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: osd
    required property var appWindow
    screen: appWindow.screen
    width: 320
    height: 88
    x: screen.virtualX + (screen.width - width) / 2
    y: screen.virtualY + screen.height - height - 80
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    color: "transparent"
    property string kind: "brightness"
    property int percent: 0
    Rectangle {
        anchors.fill: parent
        radius: 12
        color: palette.window
        border.color: palette.mid
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            Label {
                text: (osd.kind === "volume" ? qsTr("Volume") : qsTr("Brightness")) + " " + osd.percent + "%"
                Layout.fillWidth: true
                Accessible.role: Accessible.AlertMessage
            }
            ProgressBar {
                from: 0
                to: 100
                value: osd.percent
                Layout.fillWidth: true
            }
        }
    }
    SystemPalette { id: palette }
    Timer { id: hideTimer; interval: 1500; onTriggered: osd.hide() }
    Connections {
        target: desktopIntegration
        function onOsdRequested(kind, percent) {
            osd.kind = kind
            osd.percent = percent
            osd.show()
            hideTimer.restart()
        }
    }
}
