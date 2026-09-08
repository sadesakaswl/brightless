pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.brightless

ApplicationWindow {
    id: window
    width: Math.min(Screen.desktopAvailableWidth, Math.max(640, minimumWidth, monitorColumn.implicitWidth + 48))
    height: Math.min(Screen.desktopAvailableHeight, Math.max(480, minimumHeight, monitorColumn.implicitHeight + header.height + 48))
    minimumWidth: 360
    minimumHeight: 320
    visible: false
    title: "Brightless"

    BrightlessController {
        id: controller
        Component.onCompleted: initialize()
    }

    SdrBrightnessController {
        id: sdrController
        Component.onCompleted: initialize()
    }

    readonly property var backend: controller
    readonly property var sdrBackend: sdrController
    property int revision: controller.revision

    function refreshed(value) {
        window.revision
        return value
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Label {
                text: "Brightless"
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            ToolButton {
                text: "↻"
                Accessible.name: qsTr("Refresh monitors")
                enabled: !controller.loading
                onClicked: controller.initialize()
            }
            ToolButton {
                text: "⚙"
                Accessible.name: qsTr("Settings")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Open the settings window.")
                onClicked: settingsWindow.open()
            }
        }
    }

    Dialog {
        id: errorDialog
        title: qsTr("Error")
        modal: true
        standardButtons: Dialog.Ok
        visible: !controller.loading && controller.startup_error.length > 0
            && sdrController.ready && sdrController.outputs.length === 0
        Label {
            text: qsTr("Error: %1").arg(controller.startup_error)
            wrapMode: Text.WordWrap
            width: 260
        }
    }

    Osd { appWindow: window }

    SettingsWindow {
        id: settingsWindow
        controller: window.backend
        appWindow: window
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 16
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: monitorColumn
            width: parent.width
            spacing: 12

            BusyIndicator {
                running: controller.loading
                visible: running
                Layout.alignment: Qt.AlignHCenter
                Accessible.name: qsTr("Detecting displays")
            }
            Label {
                visible: controller.operation_error.length > 0
                text: qsTr("Error: %1").arg(controller.operation_error)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Repeater {
                model: sdrController.outputs
                SdrBrightnessCard {
                    required property int modelData
                    sdrController: window.sdrBackend
                    controller: window.backend
                }
            }
            Label {
                visible: sdrController.error.length > 0
                text: qsTr("Error: %1").arg(sdrController.error)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Repeater {
                model: controller.monitor_count
                MonitorCard {
                    required property int index
                    controller: window.backend
                    monitorIndex: index
                    Layout.fillWidth: true
                }
            }
        }
    }
}
