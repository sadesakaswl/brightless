import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ControlCard {
    id: hdrCard
    required property int modelData
    required property var sdrController
    required property var controller
    readonly property int currentBrightness: sdrController.brightness[hdrCard.modelData] ?? 200
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
                objectName: "sdrBrightnessSlider"
                from: hdrCard.sdrController.minimumBrightness
                to: Math.min(hdrCard.sdrController.maximumBrightness, Math.max(1000, sdrValue.value))
                stepSize: 1
                value: hdrCard.currentBrightness
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
                        const next = Math.max(hdrCard.sdrController.minimumBrightness,
                            Math.min(hdrCard.sdrController.maximumBrightness,
                                sdrValue.value + Math.sign(delta) * controller.scroll_step()))
                        if (next !== sdrValue.value)
                            sdrController.setBrightness(hdrCard.modelData, next)
                        event.accepted = true
                    }
                }
            }
            SpinBox {
                id: sdrValue
                objectName: "sdrBrightnessValue"
                from: Math.min(hdrCard.sdrController.minimumBrightness, hdrCard.currentBrightness)
                to: Math.max(hdrCard.sdrController.maximumBrightness, hdrCard.currentBrightness)
                editable: true
                value: hdrCard.currentBrightness
                Accessible.name: sdrLabel.text
                onValueModified: sdrController.setBrightness(hdrCard.modelData,
                    Math.max(hdrCard.sdrController.minimumBrightness,
                        Math.min(hdrCard.sdrController.maximumBrightness, value)))
            }
        }
    }
}
