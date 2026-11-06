import QtQuick
import QtTest
import "../src/qt/qml" as Ui

Item {
    id: root
    width: 600
    height: 300

    Component {
        id: backendComponent
        QtObject {
            property int minimumBrightness: 50
            property int maximumBrightness: 10000
            property var brightness: [200]
            property bool hide_brightness: false
            property int scrollStep: 2
            property int writes: 0
            property int lastIndex: -1
            property int lastValue: -1
            function name(index) { return "HDR display " + index }
            function scroll_step() { return scrollStep }
            function setBrightness(index, value) {
                lastIndex = index
                lastValue = value
                writes++
                const values = brightness.slice()
                values[index] = value
                brightness = values
            }
        }
    }
    Component {
        id: cardComponent
        Ui.SdrBrightnessCard {
            width: 400
            height: implicitHeight
            modelData: 0
        }
    }

    TestCase {
        name: "SdrBrightnessCard"
        when: windowShown
        property var backend
        property var card
        property var slider
        property var spin

        function init() {
            failOnWarning(/.*/)
            backend = createTemporaryObject(backendComponent, root)
            verify(!!backend, "Component exists")
            card = createTemporaryObject(cardComponent, root, { controller: backend, sdrController: backend })
            verify(!!card, "Component exists")
            slider = findChild(card, "sdrBrightnessSlider")
            verify(!!slider, "Object exists")
            spin = findChild(card, "sdrBrightnessValue")
            verify(!!spin, "Object exists")
        }

        function test_ranges_and_readback_data() {
            return [
                { tag: "windows", minimum: 80, maximum: 480, readback: 200 },
                { tag: "windows-low", minimum: 80, maximum: 480, readback: 40 },
                { tag: "windows-high", minimum: 80, maximum: 480, readback: 600 },
                { tag: "linux", minimum: 50, maximum: 10000, readback: 200 },
                { tag: "linux-dynamic", minimum: 50, maximum: 10000, readback: 2000 },
                { tag: "linux-negative", minimum: 50, maximum: 10000, readback: -20 },
                { tag: "linux-high", minimum: 50, maximum: 10000, readback: 20000 }
            ]
        }

        function test_ranges_and_readback(data) {
            backend.minimumBrightness = data.minimum
            backend.maximumBrightness = data.maximum
            backend.brightness = [data.readback]
            const sliderMaximum = Math.min(data.maximum, Math.max(1000, data.readback))
            compare(slider.from, data.minimum)
            compare(slider.to, sliderMaximum)
            compare(slider.value, Math.max(data.minimum, Math.min(sliderMaximum, data.readback)))
            compare(spin.from, Math.min(data.minimum, data.readback))
            compare(spin.to, Math.max(data.maximum, data.readback))
            compare(spin.value, data.readback)
            compare(Number.fromLocaleString(spin.locale, spin.contentItem.text), data.readback)
            compare(slider.Accessible.name, qsTr("SDR brightness (nits):"))
            compare(spin.Accessible.name, slider.Accessible.name)
            compare(backend.writes, 0)
            backend.hide_brightness = true
            compare(card.visible, false)
            compare(backend.writes, 0)
        }

        function test_input_writes_data() {
            return [
                { tag: "windows", minimum: 80, maximum: 480 },
                { tag: "linux", minimum: 50, maximum: 10000 }
            ]
        }

        function test_input_writes(data) {
            backend.minimumBrightness = data.minimum
            backend.maximumBrightness = data.maximum
            verify(waitForRendering(card))
            slider.forceActiveFocus()
            keyClick(Qt.Key_Right)
            tryCompare(backend, "lastValue", 201)
            spin.forceActiveFocus()
            keyClick(Qt.Key_Up)
            tryCompare(backend, "lastValue", 202)

            mousePress(slider, slider.width / 2, slider.height / 2)
            mouseMove(slider, slider.width * 0.75, slider.height / 2)
            mouseRelease(slider, slider.width * 0.75, slider.height / 2)
            tryVerify(() => backend.lastValue > slider.from + (slider.to - slider.from) * 0.6)
            tryVerify(() => backend.lastValue <= slider.to)
            tryCompare(backend, "lastIndex", 0)

            backend.brightness = [data.minimum]
            const beforeMinimum = backend.writes
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, -120)
            tryCompare(backend, "writes", beforeMinimum)
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, 120)
            tryCompare(backend, "lastValue", data.minimum + 2)
            backend.brightness = [data.maximum - 1]
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, 120)
            tryCompare(backend, "lastValue", data.maximum)
            const beforeMaximum = backend.writes
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, 120)
            tryCompare(backend, "writes", beforeMaximum)
            mouseWheel(slider, slider.width / 2, slider.height / 2, 120, 0)
            tryCompare(backend, "writes", beforeMaximum)

            backend.brightness = [data.maximum + 120]
            tryCompare(spin, "value", data.maximum + 120)
            spin.forceActiveFocus()
            keyClick(Qt.Key_Down)
            tryCompare(backend, "writes", beforeMaximum + 1)
            tryCompare(backend, "lastValue", data.maximum)
            tryCompare(spin, "value", data.maximum)
            backend.brightness = [data.minimum - 120]
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, 120)
            tryCompare(backend, "lastValue", data.minimum)
            tryCompare(spin, "value", data.minimum)
            backend.scrollStep = 20000
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, 120)
            tryCompare(backend, "lastValue", data.maximum)
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, -120)
            tryCompare(backend, "lastValue", data.minimum)
        }
    }
}
