import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.brightless

ApplicationWindow {
    id: window
    width: 400
    height: 300
    visible: true
    title: "Brightless"

    BrightlessController {
        id: controller
        Component.onCompleted: initialize()
    }

    property int revision: controller.revision

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
                value: controller.scroll_step()
                Layout.fillWidth: true
                onMoved: controller.set_scroll_step(Math.round(value))
            }

            Label { text: "Dynamic Contrast"; font.bold: true }

            RowLayout {
                Label { text: "Enable Dynamic Contrast"; Layout.fillWidth: true }
                Switch {
                    checked: controller.dynamic_contrast_enabled()
                    onToggled: controller.set_dynamic_contrast_enabled(checked)
                }
            }

            ColumnLayout {
                visible: controller.dynamic_contrast_enabled()
                spacing: 8

                RowLayout {
                    Label { text: "Apply to all monitors"; Layout.fillWidth: true }
                    Switch {
                        checked: controller.dynamic_contrast_global()
                        onToggled: controller.set_dynamic_contrast_global(checked)
                    }
                }

                RowLayout {
                    visible: !controller.dynamic_contrast_per_monitor_ratio()
                    Label { text: "Contrast Ratio:"; Layout.preferredWidth: 120 }
                    Label { text: ratioSlider.value.toFixed(1); Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
                }
                Slider {
                    id: ratioSlider
                    visible: !controller.dynamic_contrast_per_monitor_ratio()
                    from: 0.1
                    to: 2.0
                    stepSize: 0.1
                    value: controller.dynamic_contrast_ratio()
                    Layout.fillWidth: true
                    onMoved: controller.set_dynamic_contrast_ratio(value)
                }

                RowLayout {
                    Label { text: "Per-monitor ratio"; Layout.fillWidth: true }
                    Switch {
                        checked: controller.dynamic_contrast_per_monitor_ratio()
                        onToggled: controller.set_dynamic_contrast_per_monitor_ratio(checked)
                    }
                }

                Repeater {
                    model: controller.monitor_count()
                    ColumnLayout {
                        visible: controller.dynamic_contrast_per_monitor_ratio() && controller.supports_contrast(index)
                        RowLayout {
                            Label { text: controller.monitor_name(index) + " Ratio:"; Layout.fillWidth: true }
                            Label { text: perMonitorRatio.value.toFixed(1) }
                        }
                        Slider {
                            id: perMonitorRatio
                            from: 0.1
                            to: 2.0
                            stepSize: 0.1
                            value: controller.monitor_ratio(index)
                            Layout.fillWidth: true
                            onMoved: controller.set_monitor_ratio(index, value)
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
            width: parent.width
            spacing: 12
            Repeater {
                model: controller.monitor_count()
                MonitorCard {
                    controller: controller
                    monitorIndex: index
                    Layout.fillWidth: true
                }
            }
        }
    }
}
