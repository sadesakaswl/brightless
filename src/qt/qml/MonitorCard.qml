import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property var controller
    required property int monitorIndex
    property int revision: root.controller.revision
    readonly property bool compact: width < 560
    readonly property int labelWidth: compact ? 100 : 140
    readonly property bool showInput: !root.controller.hide_input
        && root.controller.supports_input_source(root.monitorIndex)

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
        { text: qsTr("On"), code: 1 },
        { text: qsTr("Standby"), code: 2 },
        { text: qsTr("Suspend"), code: 3 },
        { text: qsTr("Off"), code: 4 },
        { text: qsTr("Normal"), code: 5 }
    ]
    property int inputSourceCode: {
        root.revision
        return root.controller.input_source_code(root.monitorIndex)
    }
    property int powerModeCode: {
        root.revision
        return root.controller.power_mode_code(root.monitorIndex)
    }
    property var inputSourceModel: choicesWithCurrent(inputSourceChoices, inputSourceCode)
    property var powerModeModel: choicesWithCurrent(powerModeChoices, powerModeCode)

    function refreshed(value) {
        root.revision
        return value
    }

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
            text: Number(code) === 0 ? qsTr("Unknown") : qsTr("Current (%1)").arg(code),
            code: code
        }
        var result = [ fallback ]
        for (var i = 0; i < choices.length; i++) {
            result.push(choices[i])
        }
        return result
    }

    function sliderWheel(slider, wheel, applyValue) {
        var step = root.controller.scroll_step()
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
            text: root.controller.monitor_names[root.monitorIndex]
            font.bold: true
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.refreshed(root.controller.dynamic_contrast_enabled())
                && !root.controller.dynamic_contrast_global()
                && root.controller.supports_contrast(root.monitorIndex)
            Label { text: qsTr("Dynamic Contrast:"); Layout.fillWidth: true }
            Switch {
                Accessible.name: qsTr("Dynamic Contrast")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Adjust this monitor's brightness and contrast together.")
                checked: root.refreshed(root.controller.monitor_dynamic_contrast_enabled(root.monitorIndex))
                onToggled: root.controller.set_monitor_dynamic_contrast_enabled(root.monitorIndex, checked)
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.compact ? 2 : 3
            visible: !root.controller.hide_brightness
                && !root.refreshed(root.controller.monitor_dynamic_contrast_enabled(root.monitorIndex))
            Label {
                text: qsTr("Brightness:")
                Layout.columnSpan: root.compact ? 2 : 1
                Layout.preferredWidth: root.labelWidth
            }
            Slider {
                id: brightnessSlider
                from: 0
                to: 100
                stepSize: 1
                value: root.refreshed(root.controller.brightness(root.monitorIndex))
                Accessible.name: qsTr("Brightness")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Set this monitor's brightness to %1%.").arg(Math.round(value))
                Layout.fillWidth: true
                onMoved: root.controller.set_brightness(root.monitorIndex, Math.round(value))
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    onWheel: (wheel) => root.sliderWheel(brightnessSlider, wheel, function(value) { root.controller.set_brightness(root.monitorIndex, value) })
                }
            }
            Label { text: Math.round(brightnessSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.compact ? 2 : 3
            visible: !root.controller.hide_contrast
                && root.controller.supports_contrast(root.monitorIndex)
                && !root.refreshed(root.controller.monitor_dynamic_contrast_enabled(root.monitorIndex))
            Label {
                text: qsTr("Contrast:")
                Layout.columnSpan: root.compact ? 2 : 1
                Layout.preferredWidth: root.labelWidth
            }
            Slider {
                id: contrastSlider
                from: 0
                to: 100
                stepSize: 1
                value: root.refreshed(root.controller.contrast(root.monitorIndex))
                Accessible.name: qsTr("Contrast")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Set this monitor's contrast to %1%.").arg(Math.round(value))
                Layout.fillWidth: true
                onMoved: root.controller.set_contrast(root.monitorIndex, Math.round(value))
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    onWheel: (wheel) => root.sliderWheel(contrastSlider, wheel, function(value) { root.controller.set_contrast(root.monitorIndex, value) })
                }
            }
            Label { text: Math.round(contrastSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.compact ? 2 : 3
            visible: !root.controller.hide_brightness
                && root.controller.supports_contrast(root.monitorIndex)
                && root.refreshed(root.controller.monitor_dynamic_contrast_enabled(root.monitorIndex))
            Label {
                text: qsTr("Dynamic Contrast:")
                Layout.columnSpan: root.compact ? 2 : 1
                Layout.preferredWidth: root.labelWidth
            }
            Slider {
                id: dynamicContrastSlider
                from: 0
                to: 100
                stepSize: 1
                value: root.refreshed(root.controller.brightness(root.monitorIndex))
                Accessible.name: qsTr("Dynamic Contrast")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Set brightness to %1% and adjust contrast automatically.").arg(Math.round(value))
                Layout.fillWidth: true
                onMoved: root.controller.set_dynamic_contrast_brightness(root.monitorIndex, Math.round(value))
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    onWheel: (wheel) => root.sliderWheel(dynamicContrastSlider, wheel, function(value) { root.controller.set_dynamic_contrast_brightness(root.monitorIndex, value) })
                }
            }
            Label { text: Math.round(dynamicContrastSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.compact ? 2 : 3
            visible: !root.controller.hide_volume
                && root.controller.supports_volume(root.monitorIndex)
            Label {
                text: qsTr("Volume:")
                Layout.columnSpan: root.compact ? 2 : 1
                Layout.preferredWidth: root.labelWidth
            }
            Slider {
                id: volumeSlider
                from: 0
                to: 100
                stepSize: 1
                value: root.refreshed(root.controller.volume(root.monitorIndex))
                Accessible.name: qsTr("Volume")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Set this monitor's speaker volume to %1%.").arg(Math.round(value))
                Layout.fillWidth: true
                onMoved: root.controller.set_volume(root.monitorIndex, Math.round(value))
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    onWheel: (wheel) => root.sliderWheel(volumeSlider, wheel, function(value) { root.controller.set_volume(root.monitorIndex, value) })
                }
            }
            Label { text: Math.round(volumeSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.compact || !root.showInput ? 2 : 4
            visible: root.showInput || root.controller.supports_power_mode(root.monitorIndex)
            Label {
                text: qsTr("Input:")
                visible: root.showInput
                Layout.preferredWidth: root.labelWidth
            }
            ComboBox {
                visible: root.showInput
                textRole: "text"
                valueRole: "code"
                model: root.inputSourceModel
                currentIndex: root.choiceIndex(root.inputSourceModel, root.inputSourceCode)
                Layout.fillWidth: true
                onActivated: {
                    if (Number(currentValue) > 0) {
                        root.controller.set_input_source(root.monitorIndex, currentValue)
                    }
                }
            }

            Label { text: qsTr("Power:"); visible: root.controller.supports_power_mode(root.monitorIndex) }
            ComboBox {
                visible: root.controller.supports_power_mode(root.monitorIndex)
                textRole: "text"
                valueRole: "code"
                model: root.powerModeModel
                currentIndex: root.choiceIndex(root.powerModeModel, root.powerModeCode)
                Layout.fillWidth: true
                onActivated: {
                    if (Number(currentValue) > 0) {
                        root.controller.set_power_mode(root.monitorIndex, currentValue)
                    }
                }
            }
        }
    }
}
