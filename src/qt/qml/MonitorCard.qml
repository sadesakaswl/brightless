import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property var controller
    required property int monitorIndex
    property int revision: controller.revision

    property var inputSourceChoices: [
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
    property var powerModeChoices: [
        { text: "On", code: 1 },
        { text: "Standby", code: 2 },
        { text: "Suspend", code: 3 },
        { text: "Off", code: 4 },
        { text: "Normal", code: 5 }
    ]
    property int inputSourceCode: {
        root.revision
        return controller.input_source_code(root.monitorIndex)
    }
    property int powerModeCode: {
        root.revision
        return controller.power_mode_code(root.monitorIndex)
    }
    property var inputSourceModel: choicesWithCurrent(inputSourceChoices, inputSourceCode)
    property var powerModeModel: choicesWithCurrent(powerModeChoices, powerModeCode)

    function choiceIndex(choices, code) {
        for (var i = 0; i < choices.length; i++) {
            if (Number(choices[i].code) === Number(code)) {
                return i
            }
        }
        return -1
    }

    function choicesWithCurrent(choices, code) {
        if (choiceIndex(choices, code) >= 0) {
            return choices
        }

        var fallback = {
            text: Number(code) === 0 ? "Unknown" : "Current (" + code + ")",
            code: code
        }
        var result = [ fallback ]
        for (var i = 0; i < choices.length; i++) {
            result.push(choices[i])
        }
        return result
    }

    function sliderWheel(slider, wheel, applyValue) {
        var step = controller.scroll_step()
        if (wheel.angleDelta.y > 0) {
            slider.value = Math.min(slider.to, slider.value + step)
        } else if (wheel.angleDelta.y < 0) {
            slider.value = Math.max(slider.from, slider.value - step)
        }
        applyValue(Math.round(slider.value))
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
            Label { text: "Dynamic Contrast:"; Layout.preferredWidth: 140 }
            Switch {
                checked: controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
                onToggled: controller.set_monitor_dynamic_contrast_enabled(root.monitorIndex, checked)
            }
        }

        RowLayout {
            visible: !controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
            Label { text: "Brightness:"; Layout.preferredWidth: 140 }
            Slider {
                id: brightnessSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.brightness(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_brightness(root.monitorIndex, Math.round(value))
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    onWheel: (wheel) => root.sliderWheel(brightnessSlider, wheel, function(value) { controller.set_brightness(root.monitorIndex, value) })
                }
            }
            Label { text: Math.round(brightnessSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_contrast(root.monitorIndex)
                && !controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
            Label { text: "Contrast:"; Layout.preferredWidth: 140 }
            Slider {
                id: contrastSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.contrast(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_contrast(root.monitorIndex, Math.round(value))
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    onWheel: (wheel) => root.sliderWheel(contrastSlider, wheel, function(value) { controller.set_contrast(root.monitorIndex, value) })
                }
            }
            Label { text: Math.round(contrastSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_contrast(root.monitorIndex)
                && controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
            Label { text: "Dynamic Contrast:"; Layout.preferredWidth: 140 }
            Slider {
                id: dynamicContrastSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.brightness(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_dynamic_contrast_brightness(root.monitorIndex, Math.round(value))
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    onWheel: (wheel) => root.sliderWheel(dynamicContrastSlider, wheel, function(value) { controller.set_dynamic_contrast_brightness(root.monitorIndex, value) })
                }
            }
            Label { text: Math.round(dynamicContrastSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_volume(root.monitorIndex)
            Label { text: "Volume:"; Layout.preferredWidth: 140 }
            Slider {
                id: volumeSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.volume(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_volume(root.monitorIndex, Math.round(value))
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    onWheel: (wheel) => root.sliderWheel(volumeSlider, wheel, function(value) { controller.set_volume(root.monitorIndex, value) })
                }
            }
            Label { text: Math.round(volumeSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_input_source(root.monitorIndex) || controller.supports_power_mode(root.monitorIndex)
            Label { text: "Input:"; Layout.preferredWidth: 140; visible: controller.supports_input_source(root.monitorIndex) }
            ComboBox {
                visible: controller.supports_input_source(root.monitorIndex)
                textRole: "text"
                valueRole: "code"
                model: root.inputSourceModel
                currentIndex: root.choiceIndex(root.inputSourceModel, root.inputSourceCode)
                Layout.fillWidth: true
                onActivated: {
                    if (Number(currentValue) > 0) {
                        controller.set_input_source(root.monitorIndex, currentValue)
                    }
                }
            }

            Label { text: "Power:"; visible: controller.supports_power_mode(root.monitorIndex) }
            ComboBox {
                visible: controller.supports_power_mode(root.monitorIndex)
                textRole: "text"
                valueRole: "code"
                model: root.powerModeModel
                currentIndex: root.choiceIndex(root.powerModeModel, root.powerModeCode)
                Layout.fillWidth: true
                onActivated: {
                    if (Number(currentValue) > 0) {
                        controller.set_power_mode(root.monitorIndex, currentValue)
                    }
                }
            }
        }
    }
}
