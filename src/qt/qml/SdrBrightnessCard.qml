import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: hdrCard
    required property int modelData
    required property var sdrController
    required property var controller
    Layout.fillWidth: true
    visible: !controller.hide_brightness

    ColumnLayout {
        anchors.fill: parent
        Label {
            text: qsTr("%1 — HDR").arg(sdrController.name(hdrCard.modelData))
            font.bold: true
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
                Accessible.name: sdrLabel.text
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Adjust the brightness of SDR content on this HDR screen.")
                onMoved: sdrController.setBrightness(hdrCard.modelData, Math.round(value))
                WheelHandler {
                    onWheel: (event) => {
                        if (event.angleDelta.y !== 0) {
                            sdrController.setBrightness(hdrCard.modelData,
                                sdrValue.value + Math.sign(event.angleDelta.y) * controller.scroll_step())
                            event.accepted = true
                        }
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
