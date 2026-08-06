pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.brightless

ApplicationWindow {
    id: window
    width: Math.min(Screen.desktopAvailableWidth, Math.max(minimumWidth, monitorColumn.implicitWidth + 48))
    height: Math.min(Screen.desktopAvailableHeight, Math.max(minimumHeight, monitorColumn.implicitHeight + header.height + 48))
    minimumWidth: 480
    minimumHeight: 320
    visible: true
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
                onClicked: settingsPopup.open()
            }
        }
    }

    Dialog {
        id: errorDialog
        title: "Error"
        modal: true
        standardButtons: Dialog.Ok
        visible: controller.startup_error.length > 0
        Label {
            text: "Error: " + controller.startup_error
            wrapMode: Text.WordWrap
            width: 260
        }
    }

    Popup {
        id: settingsPopup
        modal: false
        focus: true
        x: Math.max(0, window.width - width - 12)
        y: 12
        width: 340
        padding: 12

        ColumnLayout {
            spacing: 12
            anchors.fill: parent

            Label { text: "Scroll Step:" }
            Label { text: scrollStepSlider.value.toFixed(0) + "%"; Layout.alignment: Qt.AlignRight }
            Slider {
                id: scrollStepSlider
                from: 1
                to: 10
                stepSize: 1
                value: window.refreshed(controller.scroll_step())
                Layout.fillWidth: true
                onMoved: controller.set_scroll_step(Math.round(value))
            }

            Label { text: "Dynamic Contrast"; font.bold: true }

            RowLayout {
                Label { text: "Enable Dynamic Contrast"; Layout.fillWidth: true }
                Switch {
                    checked: window.refreshed(controller.dynamic_contrast_enabled())
                    onToggled: controller.set_dynamic_contrast_enabled(checked)
                }
            }

            ColumnLayout {
                visible: window.refreshed(controller.dynamic_contrast_enabled())
                spacing: 8

                RowLayout {
                    Label { text: "Apply to all monitors"; Layout.fillWidth: true }
                    Switch {
                        checked: window.refreshed(controller.dynamic_contrast_global())
                        onToggled: controller.set_dynamic_contrast_global(checked)
                    }
                }

                RowLayout {
                    visible: !window.refreshed(controller.dynamic_contrast_per_monitor_ratio())
                    Label { text: "Contrast Ratio:"; Layout.preferredWidth: 120 }
                    Label { text: ratioSlider.value.toFixed(1); Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
                }
                Slider {
                    id: ratioSlider
                    visible: !window.refreshed(controller.dynamic_contrast_per_monitor_ratio())
                    from: 0.1
                    to: 2.0
                    stepSize: 0.1
                    value: window.refreshed(controller.dynamic_contrast_ratio())
                    Layout.fillWidth: true
                    onMoved: controller.set_dynamic_contrast_ratio(value)
                }

                RowLayout {
                    Label { text: "Per-monitor ratio"; Layout.fillWidth: true }
                    Switch {
                        checked: window.refreshed(controller.dynamic_contrast_per_monitor_ratio())
                        onToggled: controller.set_dynamic_contrast_per_monitor_ratio(checked)
                    }
                }

                Repeater {
                    model: controller.monitor_count
                    ColumnLayout {
                        id: ratioDelegate
                        required property int index
                        visible: window.refreshed(window.backend.dynamic_contrast_per_monitor_ratio())
                            && window.backend.supports_contrast(ratioDelegate.index)
                        RowLayout {
                            Label { text: window.backend.monitor_names[ratioDelegate.index] + " Ratio:"; Layout.fillWidth: true }
                            Label { text: perMonitorRatio.value.toFixed(1) }
                        }
                        Slider {
                            id: perMonitorRatio
                            from: 0.1
                            to: 2.0
                            stepSize: 0.1
                            value: window.refreshed(window.backend.monitor_ratio(ratioDelegate.index))
                            Layout.fillWidth: true
                            onMoved: window.backend.set_monitor_ratio(ratioDelegate.index, value)
                        }
                    }
                }
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 16
        ColumnLayout {
            id: monitorColumn
            width: Math.max(parent.width, implicitWidth)
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
