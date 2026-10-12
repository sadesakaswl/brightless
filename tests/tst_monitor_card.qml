import QtQuick
import QtTest
import "../src/qt/qml" as Ui

Item {
    id: root
    width: 640
    height: 600

    Component { id: backendComponent; FakeController {} }
    Component {
        id: cardComponent
        Ui.MonitorCard {
            width: 600
            height: implicitHeight
            monitorIndex: 0
        }
    }

    TestCase {
        name: "MonitorCard"
        when: windowShown

        function init() { failOnWarning(/.*/) }

        function test_dynamic_contrast_and_visibility() {
            const backend = createTemporaryObject(backendComponent, root)
            const card = createTemporaryObject(cardComponent, root, { controller: backend })
            verify(!!card, "Component exists")
            const brightness = findChild(card, "brightnessControl")
            const contrast = findChild(card, "contrastControl")
            const volume = findChild(card, "volumeControl")
            const toggle = findChild(card, "dynamicContrastSwitch")
            verify(!!brightness && !!contrast && !!volume && !!toggle, "Object exists")
            const slider = findChild(brightness, "slider")
            verify(!!slider, "Object exists")
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, 120)
            tryCompare(backend, "brightnessValue", 52)
            tryCompare(backend, "lastWrite", "brightness")
            backend.set_dynamic_contrast_enabled(true)
            tryCompare(card, "dynamicContrast", true)
            tryCompare(contrast, "visible", false)
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, -120)
            tryCompare(backend, "brightnessValue", 50)
            tryCompare(backend, "lastWrite", "dynamic")
            backend.set_dynamic_contrast_global(false)
            tryCompare(toggle, "visible", true)
            mouseClick(toggle)
            tryCompare(backend, "perMonitorDynamic", true)
            backend.hide_brightness = true
            backend.hide_volume = true
            tryCompare(brightness, "visible", false)
            tryCompare(volume, "visible", false)
            backend.hasContrast = false
            backend.revision++
            tryCompare(card, "dynamicContrast", false)
            tryCompare(toggle, "visible", false)
        }

        function test_choices_and_narrow_layout() {
            const backend = createTemporaryObject(backendComponent, root)
            const card = createTemporaryObject(cardComponent, root, { controller: backend })
            verify(!!card, "Component exists")
            backend.set_input_source(0, 99)
            backend.set_power_mode(0, 0)
            compare(card.inputSourceModel[0].code, 99)
            compare(card.powerModeModel[0].code, 0)
            const input = findChild(card, "inputSelector")
            const power = findChild(card, "powerSelector")
            verify(!!input && !!power, "Object exists")
            input.currentIndex = card.choiceIndex(card.inputSourceModel, 27)
            input.activated(input.currentIndex)
            tryCompare(backend, "inputCode", 27)
            power.currentIndex = card.choiceIndex(card.powerModeModel, 4)
            power.activated(power.currentIndex)
            tryCompare(backend, "powerCode", 4)
            card.width = 328
            tryCompare(card, "compact", true)
            tryVerify(() => power.mapToItem(card, 0, 0).y > input.mapToItem(card, 0, 0).y)
            backend.hide_input = true
            tryCompare(input, "visible", false)
            tryCompare(power, "visible", true)
            backend.hasPower = false
            backend.revision++
            tryCompare(power, "visible", false)
        }
    }
}
