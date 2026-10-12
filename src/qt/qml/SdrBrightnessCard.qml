import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ControlCard {
    id: hdrCard
    required property int modelData
    required property var sdrController
    required property var controller
    Layout.fillWidth: true
    visible: !controller.hide_brightness

    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        Label {
            text: qsTr("%1 — HDR").arg(sdrController.name(hdrCard.modelData))
            font.weight: Font.Medium
            font.pointSize: hdrCard.font.pointSize * 1.125
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            id: sdrLabel
            text: qsTr("SDR brightness (nits):")
        }
        RowLayout {
            Layout.fillWidth: true
            Slider {
                id: sdrSlider
                from: 50
                to: Math.max(1000, sdrValue.value)
                stepSize: 1
                value: sdrController.brightness[hdrCard.modelData] ?? 200
                Layout.fillWidth: true
                Layout.minimumHeight: 40
                Accessible.name: sdrLabel.text
                ToolTip.visible: hovered || activeFocus
                ToolTip.text: qsTr("Adjust the brightness of SDR content on this HDR screen.")
                onMoved: sdrController.setBrightness(hdrCard.modelData, Math.round(value))
                WheelHandler {
                    target: null
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: (event) => {
                        const delta = event.angleDelta.y || event.pixelDelta.y
                        if (delta === 0) {
                            event.accepted = false
                            return
                        }
                        sdrController.setBrightness(hdrCard.modelData,
                            sdrValue.value + Math.sign(delta) * controller.scroll_step())
                        event.accepted = true
                    }
                }
            }
            SpinBox {
                id: sdrValue
                from: 50
                to: 10000
                editable: true
                value: sdrController.brightness[hdrCard.modelData] ?? 200
                Accessible.name: sdrLabel.text
                onValueModified: sdrController.setBrightness(hdrCard.modelData, value)
            }
        }
    }
}
