import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property var controller
    required property int monitorIndex
    property int revision: controller.revision

    function sliderWheel(slider, wheel) {
        var step = controller.scroll_step()
        if (wheel.angleDelta.y > 0) {
            slider.value = Math.min(slider.to, slider.value + step)
        } else if (wheel.angleDelta.y < 0) {
            slider.value = Math.max(slider.from, slider.value - step)
        }
        slider.moved()
        wheel.accepted = true
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            text: controller.monitor_names[root.monitorIndex]
            font.bold: true
            Layout.fillWidth: true
        }

        RowLayout {
            visible: controller.dynamic_contrast_enabled()
                && !controller.dynamic_contrast_global()
                && controller.supports_contrast(root.monitorIndex)
            Label { text: "Dynamic Contrast:"; Layout.preferredWidth: 120 }
            Switch {
                checked: controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
                onToggled: controller.set_monitor_dynamic_contrast_enabled(root.monitorIndex, checked)
            }
        }

        RowLayout {
            visible: !controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
            Label { text: "Brightness:"; Layout.preferredWidth: 120 }
            Slider {
                id: brightnessSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.brightness(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_brightness(root.monitorIndex, Math.round(value))
                WheelHandler { onWheel: root.sliderWheel(brightnessSlider, wheel) }
            }
            Label { text: Math.round(brightnessSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_contrast(root.monitorIndex)
                && !controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
            Label { text: "Contrast:"; Layout.preferredWidth: 120 }
            Slider {
                id: contrastSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.contrast(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_contrast(root.monitorIndex, Math.round(value))
                WheelHandler { onWheel: root.sliderWheel(contrastSlider, wheel) }
            }
            Label { text: Math.round(contrastSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_contrast(root.monitorIndex)
                && controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
            Label { text: "Dynamic Contrast:"; Layout.preferredWidth: 120 }
            Slider {
                id: dynamicContrastSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.brightness(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_dynamic_contrast_brightness(root.monitorIndex, Math.round(value))
                WheelHandler { onWheel: root.sliderWheel(dynamicContrastSlider, wheel) }
            }
            Label { text: Math.round(dynamicContrastSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_volume(root.monitorIndex)
            Label { text: "Volume:"; Layout.preferredWidth: 120 }
            Slider {
                id: volumeSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.volume(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_volume(root.monitorIndex, Math.round(value))
                WheelHandler { onWheel: root.sliderWheel(volumeSlider, wheel) }
            }
            Label { text: Math.round(volumeSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_input_source(root.monitorIndex) || controller.supports_power_mode(root.monitorIndex)
            Label { text: "Input:"; Layout.preferredWidth: 120; visible: controller.supports_input_source(root.monitorIndex) }
            ComboBox {
                visible: controller.supports_input_source(root.monitorIndex)
                textRole: "text"
                valueRole: "code"
                model: [
                    { text: "VGA", code: 1 },
                    { text: "DVI", code: 3 },
                    { text: "DisplayPort 1", code: 15 },
                    { text: "DisplayPort 2", code: 16 },
                    { text: "HDMI 1", code: 17 },
                    { text: "HDMI 2", code: 18 },
                    { text: "HDMI 3", code: 19 },
                    { text: "HDMI 4", code: 20 },
                    { text: "USB-C", code: 27 }
                ]
                onActivated: controller.set_input_source(root.monitorIndex, currentValue)
            }

            Label { text: "Power:"; visible: controller.supports_power_mode(root.monitorIndex) }
            ComboBox {
                visible: controller.supports_power_mode(root.monitorIndex)
                textRole: "text"
                valueRole: "code"
                model: [
                    { text: "On", code: 1 },
                    { text: "Standby", code: 2 },
                    { text: "Suspend", code: 3 },
                    { text: "Off", code: 4 },
                    { text: "Normal", code: 5 }
                ]
                onActivated: controller.set_power_mode(root.monitorIndex, currentValue)
            }
        }
    }
}
