pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.brightless

ApplicationWindow {
    id: window
    width: Math.min(Screen.desktopAvailableWidth, Math.max(minimumWidth, monitorColumn.implicitWidth + 48))
    height: Math.min(Screen.desktopAvailableHeight, Math.max(minimumHeight, monitorColumn.implicitHeight + header.height + 48))
    minimumWidth: 360
    minimumHeight: 320
    visible: false
    title: "Brightless"

    BrightlessController {
        id: controller
        Component.onCompleted: initialize()
    }

    readonly property var backend: controller
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
        visible: controller.startup_error.length > 0
        Label {
            text: qsTr("Error: %1").arg(controller.startup_error)
            wrapMode: Text.WordWrap
            width: 260
        }
    }

    ApplicationWindow {
        id: settingsWindow
        title: qsTr("Settings")
        visible: false
        flags: Qt.Dialog
        transientParent: window
        width: Math.min(420, window.width - 24)
        height: Math.min(520, window.height - 24)
        color: palette.window

        function open() {
            x = window.x + Math.round((window.width - width) / 2)
            y = window.y + Math.round((window.height - height) / 2)
            show()
            requestActivate()
        }

        onActiveChanged: {
            if (visible && !active)
                Qt.callLater(() => close())
        }

        Shortcut {
            sequence: "Esc"
            onActivated: settingsWindow.close()
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            TabBar {
                id: settingsTabs
                Layout.fillWidth: true
                TabButton { text: qsTr("Appearance"); ToolTip.visible: hovered; ToolTip.text: qsTr("Choose which monitor controls and icons are shown.") }
                TabButton { text: qsTr("Behaviour"); ToolTip.visible: hovered; ToolTip.text: qsTr("Adjust scrolling, DDC timing, and dynamic contrast.") }
                TabButton { text: qsTr("System"); ToolTip.visible: hovered; ToolTip.text: qsTr("Configure startup and window-closing behaviour.") }
                TabButton { text: qsTr("About"); ToolTip.visible: hovered; ToolTip.text: qsTr("View version, source code, and license information.") }
            }

            StackLayout {
                currentIndex: settingsTabs.currentIndex
                Layout.fillWidth: true
                Layout.fillHeight: true

                Item {
                    ColumnLayout {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                id: hideBrightnessLabel
                                text: window.refreshed(controller.dynamic_contrast_enabled())
                                    ? qsTr("Hide Dynamic Contrast Option")
                                    : qsTr("Hide Brightness Option")
                                Layout.fillWidth: true
                            }
                            Switch {
                                Accessible.name: hideBrightnessLabel.text
                                ToolTip.visible: hovered
                                ToolTip.text: window.refreshed(controller.dynamic_contrast_enabled())
                                    ? qsTr("Remove the dynamic contrast control from monitor cards.")
                                    : qsTr("Remove the brightness control from monitor cards.")
                                checked: controller.hide_brightness
                                onToggled: controller.hide_brightness = checked
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: !window.refreshed(controller.dynamic_contrast_enabled())
                            Label { text: qsTr("Hide Contrast Option"); Layout.fillWidth: true }
                            Switch {
                                Accessible.name: qsTr("Hide Contrast Option")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Remove the contrast control from monitor cards.")
                                checked: controller.hide_contrast
                                onToggled: controller.hide_contrast = checked
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("Hide Volume Option"); Layout.fillWidth: true }
                            Switch {
                                Accessible.name: qsTr("Hide Volume Option")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Remove the volume control from monitor cards.")
                                checked: controller.hide_volume
                                onToggled: controller.hide_volume = checked
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("Hide Input Option"); Layout.fillWidth: true }
                            Switch {
                                Accessible.name: qsTr("Hide Input Option")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Remove the input-source control from monitor cards.")
                                checked: controller.hide_input
                                onToggled: controller.hide_input = checked
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("Hide Tray Icon"); Layout.fillWidth: true }
                            Switch {
                                Accessible.name: qsTr("Hide Tray Icon")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Remove Brightless from the system tray.")
                                checked: controller.hide_tray_icon
                                onToggled: controller.hide_tray_icon = checked
                            }
                        }
                    }
                }

                ScrollView {
                    id: behaviourScroll
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: behaviourScroll.availableWidth
                        spacing: 12

                        Label { text: qsTr("Scroll Step:") }
                        Label { text: scrollStepSlider.value.toFixed(0) + "%"; Layout.alignment: Qt.AlignRight }
                        Slider {
                            id: scrollStepSlider
                            from: 1
                            to: 10
                            stepSize: 1
                            value: window.refreshed(controller.scroll_step())
                            Accessible.name: qsTr("Scroll Step")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Change monitor values by %1% for each mouse-wheel step.").arg(Math.round(value))
                            Layout.fillWidth: true
                            onMoved: controller.set_scroll_step(Math.round(value))
                        }

                        Label { id: ddcDelayLabel; text: qsTr("Delay to send DDC signal") }
                        Label {
                            text: Math.round(ddcDelaySlider.value) === 0
                                ? qsTr("Instant")
                                : Math.round(ddcDelaySlider.value) + " ms"
                            Layout.alignment: Qt.AlignRight
                        }
                        Slider {
                            id: ddcDelaySlider
                            from: 0
                            to: 1500
                            stepSize: 50
                            value: window.refreshed(controller.ddc_delay())
                            Accessible.name: ddcDelayLabel.text
                            ToolTip.visible: hovered
                            ToolTip.text: Math.round(value) === 0
                                ? qsTr("Send monitor control updates immediately.")
                                : qsTr("Wait %1 ms before sending monitor control updates.").arg(Math.round(value))
                            Layout.fillWidth: true
                            onMoved: controller.set_ddc_delay(Math.round(value))
                        }

                        Label { text: qsTr("Dynamic Contrast"); font.bold: true }

                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("Enable Dynamic Contrast"); Layout.fillWidth: true }
                            Switch {
                                Accessible.name: qsTr("Enable Dynamic Contrast")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Adjust brightness and contrast together using a configurable ratio.")
                                checked: window.refreshed(controller.dynamic_contrast_enabled())
                                onToggled: controller.set_dynamic_contrast_enabled(checked)
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: window.refreshed(controller.dynamic_contrast_enabled())
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: qsTr("Apply to all monitors"); Layout.fillWidth: true }
                                Switch {
                                    Accessible.name: qsTr("Apply to all monitors")
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Use dynamic contrast on every compatible monitor.")
                                    checked: window.refreshed(controller.dynamic_contrast_global())
                                    onToggled: controller.set_dynamic_contrast_global(checked)
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                visible: !window.refreshed(controller.dynamic_contrast_per_monitor_ratio())
                                Label { text: qsTr("Contrast Ratio:"); Layout.preferredWidth: 120 }
                                Label { text: ratioSlider.value.toFixed(1); Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
                            }
                            Slider {
                                id: ratioSlider
                                visible: !window.refreshed(controller.dynamic_contrast_per_monitor_ratio())
                                from: 0.1
                                to: 2.0
                                stepSize: 0.1
                                value: window.refreshed(controller.dynamic_contrast_ratio())
                                Accessible.name: qsTr("Contrast Ratio")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Set contrast to %1 times brightness.").arg(value.toFixed(1))
                                Layout.fillWidth: true
                                onMoved: controller.set_dynamic_contrast_ratio(value)
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: qsTr("Per-monitor ratio"); Layout.fillWidth: true }
                                Switch {
                                    Accessible.name: qsTr("Per-monitor ratio")
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Allow each monitor to use its own contrast ratio.")
                                    checked: window.refreshed(controller.dynamic_contrast_per_monitor_ratio())
                                    onToggled: controller.set_dynamic_contrast_per_monitor_ratio(checked)
                                }
                            }

                            Repeater {
                                model: controller.monitor_count
                                ColumnLayout {
                                    id: ratioDelegate
                                    required property int index
                                    Layout.fillWidth: true
                                    visible: window.refreshed(window.backend.dynamic_contrast_per_monitor_ratio())
                                        && window.backend.supports_contrast(ratioDelegate.index)
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: qsTr("%1 Ratio:").arg(window.backend.monitor_names[ratioDelegate.index])
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        Label { text: perMonitorRatio.value.toFixed(1) }
                                    }
                                    Slider {
                                        id: perMonitorRatio
                                        from: 0.1
                                        to: 2.0
                                        stepSize: 0.1
                                        value: window.refreshed(window.backend.monitor_ratio(ratioDelegate.index))
                                        Accessible.name: qsTr("%1 Ratio").arg(window.backend.monitor_names[ratioDelegate.index])
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Set this monitor's contrast to %1 times its brightness.").arg(value.toFixed(1))
                                        Layout.fillWidth: true
                                        onMoved: window.backend.set_monitor_ratio(ratioDelegate.index, value)
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("Autostart on login"); Layout.fillWidth: true }
                            Switch {
                                Accessible.name: qsTr("Autostart on login")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Launch Brightless automatically when you log in.")
                                checked: controller.autostart
                                onToggled: controller.autostart = checked
                            }
                        }

                        RowLayout {
                            id: autostartTrayRow
                            readonly property bool optionEnabled: controller.autostart && !controller.hide_tray_icon
                            Layout.fillWidth: true
                            ToolTip.visible: autostartTrayHover.hovered
                            ToolTip.text: qsTr("Start Brightless on login without opening its window.")
                            HoverHandler { id: autostartTrayHover }
                            Label {
                                text: qsTr("Autostart as tray icon")
                                enabled: autostartTrayRow.optionEnabled
                                Layout.fillWidth: true
                            }
                            Switch {
                                Accessible.name: qsTr("Autostart as tray icon")
                                enabled: autostartTrayRow.optionEnabled
                                checked: controller.autostart_as_tray_icon
                                onToggled: controller.autostart_as_tray_icon = checked
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            enabled: !controller.hide_tray_icon
                            Label { text: qsTr("Close to tray icon"); Layout.fillWidth: true }
                            Switch {
                                Accessible.name: qsTr("Close to tray icon")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Keep Brightless running after its window closes.")
                                checked: controller.close_to_tray
                                onToggled: controller.close_to_tray = checked
                            }
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 8

                        Image {
                            source: "qrc:/qt/qml/com/brightless/icon.png"
                            fillMode: Image.PreserveAspectFit
                            Layout.preferredWidth: 96
                            Layout.preferredHeight: 96
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Label {
                            text: "Brightless"
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Label {
                            text: qsTr("Version %1").arg(Qt.application.version)
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Label {
                            text: "<a href=\"https://github.com/sadesakaswl/brightless\">github.com/sadesakaswl/brightless</a>"
                            textFormat: Text.RichText
                            Accessible.name: qsTr("Brightless repository")
                            Layout.alignment: Qt.AlignHCenter
                            onLinkActivated: (link) => Qt.openUrlExternally(link)
                        }
                        Label {
                            text: qsTr("GNU General Public License v3.0")
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }
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
