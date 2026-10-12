pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: control
    required property string label
    required property string toolTipText
    required property int scrollStep
    property alias value: slider.value
    signal valueEdited(int value)
    spacing: 0

    RowLayout {
        Layout.fillWidth: true
        Label {
            text: control.label
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            text: Math.round(slider.value) + "%"
            font.weight: Font.Medium
        }
    }

    Slider {
        id: slider
        objectName: "slider"
        from: 0
        to: 100
        stepSize: 1
        Layout.fillWidth: true
        Layout.minimumHeight: 40
        Accessible.name: control.label
        ToolTip.visible: hovered || activeFocus
        ToolTip.text: control.toolTipText
        onMoved: control.valueEdited(Math.round(value))

        WheelHandler {
            objectName: "wheelHandler"
            target: null
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (event) => {
                const delta = event.angleDelta.y || event.pixelDelta.y
                if (delta === 0) {
                    event.accepted = false
                    return
                }
                const next = Math.max(slider.from, Math.min(slider.to,
                    slider.value + Math.sign(delta) * control.scrollStep))
                if (next !== slider.value)
                    control.valueEdited(Math.round(next))
                event.accepted = true
            }
        }
    }
}
