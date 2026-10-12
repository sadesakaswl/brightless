pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ControlCard {
    id: root

    required property var controller
    required property int monitorIndex
    readonly property int revision: root.controller.revision
    readonly property bool compact: width < 480
    readonly property bool supportsContrast: root.refreshed(root.controller.supports_contrast(root.monitorIndex))
    readonly property bool dynamicContrast: root.refreshed(root.controller.monitor_dynamic_contrast_enabled(root.monitorIndex))
    readonly property int scrollStep: root.refreshed(root.controller.scroll_step())
    readonly property bool showInput: !root.controller.hide_input
        && root.refreshed(root.controller.supports_input_source(root.monitorIndex))
    readonly property bool showPower: root.refreshed(root.controller.supports_power_mode(root.monitorIndex))

    readonly property var inputSourceChoices: [
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
    readonly property var powerModeChoices: [
        { text: qsTr("On"), code: 1 },
        { text: qsTr("Standby"), code: 2 },
        { text: qsTr("Suspend"), code: 3 },
        { text: qsTr("Off"), code: 4 },
        { text: qsTr("Normal"), code: 5 }
    ]
    readonly property int inputSourceCode: root.refreshed(root.controller.input_source_code(root.monitorIndex))
    readonly property int powerModeCode: root.refreshed(root.controller.power_mode_code(root.monitorIndex))
    readonly property var inputSourceModel: choicesWithCurrent(inputSourceChoices, inputSourceCode)
    readonly property var powerModeModel: choicesWithCurrent(powerModeChoices, powerModeCode)

    function refreshed(value) {
        root.revision
        return value
    }

    function choiceIndex(choices, code) {
        return choices.findIndex(choice => choice.code === code)
    }

    function choicesWithCurrent(choices, code) {
        if (choiceIndex(choices, code) >= 0)
            return choices
        return [{ text: code === 0 ? qsTr("Unknown") : qsTr("Current (%1)").arg(code), code: code }].concat(choices)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            text: root.controller.monitor_names[root.monitorIndex]
            font.weight: Font.Medium
            font.pointSize: root.font.pointSize * 1.125
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.refreshed(root.controller.dynamic_contrast_enabled())
                && !root.controller.dynamic_contrast_global() && root.supportsContrast
            Label { text: qsTr("Dynamic Contrast:"); Layout.fillWidth: true }
            Switch {
                objectName: "dynamicContrastSwitch"
                Accessible.name: qsTr("Dynamic Contrast")
                ToolTip.visible: hovered || activeFocus
                ToolTip.text: qsTr("Adjust this monitor's brightness and contrast together.")
                checked: root.dynamicContrast
                onToggled: root.controller.set_monitor_dynamic_contrast_enabled(root.monitorIndex, checked)
            }
        }

        MonitorSlider {
            id: brightnessControl
            objectName: "brightnessControl"
            Layout.fillWidth: true
            visible: !root.controller.hide_brightness
            label: root.dynamicContrast ? qsTr("Dynamic Contrast:") : qsTr("Brightness:")
            value: root.refreshed(root.controller.brightness(root.monitorIndex))
            scrollStep: root.scrollStep
            toolTipText: root.dynamicContrast
                ? qsTr("Set brightness to %1% and adjust contrast automatically.").arg(Math.round(value))
                : qsTr("Set this monitor's brightness to %1%.").arg(Math.round(value))
            onValueEdited: (value) => {
                if (root.dynamicContrast)
                    root.controller.set_dynamic_contrast_brightness(root.monitorIndex, value)
                else
                    root.controller.set_brightness(root.monitorIndex, value)
            }
        }

        MonitorSlider {
            objectName: "contrastControl"
            Layout.fillWidth: true
            visible: !root.controller.hide_contrast && root.supportsContrast && !root.dynamicContrast
            label: qsTr("Contrast:")
            value: root.refreshed(root.controller.contrast(root.monitorIndex))
            scrollStep: root.scrollStep
            toolTipText: qsTr("Set this monitor's contrast to %1%.").arg(Math.round(value))
            onValueEdited: (value) => root.controller.set_contrast(root.monitorIndex, value)
        }

        MonitorSlider {
            objectName: "volumeControl"
            Layout.fillWidth: true
            visible: !root.controller.hide_volume
                && root.refreshed(root.controller.supports_volume(root.monitorIndex))
            label: qsTr("Volume:")
            value: root.refreshed(root.controller.volume(root.monitorIndex))
            scrollStep: root.scrollStep
            toolTipText: qsTr("Set this monitor's speaker volume to %1%.").arg(Math.round(value))
            onValueEdited: (value) => root.controller.set_volume(root.monitorIndex, value)
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.compact || !root.showInput || !root.showPower ? 2 : 4
            columnSpacing: 12
            rowSpacing: 8
            visible: root.showInput || root.showPower
            Label { text: qsTr("Input:"); visible: root.showInput }
            ComboBox {
                objectName: "inputSelector"
                Accessible.name: qsTr("Input:")
                visible: root.showInput
                textRole: "text"
                valueRole: "code"
                model: root.inputSourceModel
                currentIndex: root.choiceIndex(root.inputSourceModel, root.inputSourceCode)
                Layout.fillWidth: true
                onActivated: {
                    if (currentValue > 0)
                        root.controller.set_input_source(root.monitorIndex, currentValue)
                }
            }

            Label { text: qsTr("Power:"); visible: root.showPower }
            ComboBox {
                objectName: "powerSelector"
                Accessible.name: qsTr("Power:")
                visible: root.showPower
                textRole: "text"
                valueRole: "code"
                model: root.powerModeModel
                currentIndex: root.choiceIndex(root.powerModeModel, root.powerModeCode)
                Layout.fillWidth: true
                onActivated: {
                    if (currentValue > 0)
                        root.controller.set_power_mode(root.monitorIndex, currentValue)
                }
            }
        }
    }
}
