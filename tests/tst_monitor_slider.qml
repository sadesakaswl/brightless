import QtQuick
import QtQuick.Controls
import QtTest
import "../src/qt/qml" as Ui

Item {
    id: root
    width: 600
    height: 200

    Component {
        id: sliderComponent
        Ui.MonitorSlider {
            id: control
            width: 320
            height: 80
            label: qsTr("Brightness")
            toolTipText: qsTr("Brightness")
            scrollStep: 2
            value: 50
            onValueEdited: (value) => control.value = value
        }
    }

    Component {
        id: scrolledSliderComponent
        ScrollView {
            id: scroll
            width: 340
            height: 150
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            Column {
                width: scroll.availableWidth
                Ui.MonitorSlider {
                    id: control
                    objectName: "monitorControl"
                    width: 320
                    height: 80
                    label: qsTr("Brightness")
                    toolTipText: qsTr("Brightness")
                    scrollStep: 2
                    value: 50
                    onValueEdited: (value) => control.value = value
                }
                Item { width: 320; height: 600 }
            }
        }
    }

    SignalSpy {
        id: valueEditedSpy
        signalName: "valueEdited"
    }

    TestCase {
        name: "MonitorSlider"
        when: windowShown

        function init() { failOnWarning(/.*/) }

        function test_wheel_step_and_limits() {
            const control = createTemporaryObject(sliderComponent, root)
            verify(!!control, "Component exists")
            const slider = findChild(control, "slider")
            verify(!!slider, "Object exists")
            const wheel = findChild(control, "wheelHandler")
            verify(!!wheel, "Object exists")
            compare(wheel.acceptedDevices, PointerDevice.Mouse | PointerDevice.TouchPad)
            valueEditedSpy.target = control
            valueEditedSpy.clear()
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, 120)
            tryCompare(control, "value", 52)
            tryCompare(valueEditedSpy, "count", 1)
            control.scrollStep = 10
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, -120)
            tryCompare(control, "value", 42)
            control.value = 99
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, 120)
            tryCompare(control, "value", 100)
            valueEditedSpy.clear()
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, 120)
            tryCompare(control, "value", 100)
            tryCompare(valueEditedSpy, "count", 0)
            control.value = 1
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, -120)
            tryCompare(control, "value", 0)
            mouseWheel(slider, slider.width / 2, slider.height / 2, 120, 0)
            tryCompare(control, "value", 0)
        }

        function test_wheel_changes_value_in_scroll_view() {
            const scroll = createTemporaryObject(scrolledSliderComponent, root)
            verify(!!scroll, "Component exists")
            const control = findChild(scroll, "monitorControl")
            verify(!!control, "Object exists")
            const slider = findChild(control, "slider")
            verify(!!slider, "Object exists")
            const flickable = scroll.contentItem
            const scrollPosition = flickable.contentY
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, 120)
            tryCompare(control, "value", 52)
            tryCompare(flickable, "contentY", scrollPosition)
            control.value = 100
            mouseWheel(slider, slider.width / 2, slider.height / 2, 0, 120)
            tryCompare(control, "value", 100)
            tryCompare(flickable, "contentY", scrollPosition)
            mouseWheel(scroll, 10, scroll.height - 10, 0, -120)
            tryVerify(() => flickable.contentY > scrollPosition)
        }

        function test_drag_and_keyboard() {
            const control = createTemporaryObject(sliderComponent, root)
            verify(!!control, "Component exists")
            const slider = findChild(control, "slider")
            verify(!!slider, "Object exists")
            mousePress(slider, slider.width / 2, slider.height / 2)
            mouseMove(slider, slider.width * 0.75, slider.height / 2)
            mouseRelease(slider, slider.width * 0.75, slider.height / 2)
            tryVerify(() => control.value > 65 && control.value < 85)
            slider.forceActiveFocus()
            const before = control.value
            keyClick(Qt.Key_Right)
            tryCompare(control, "value", before + 1)
        }
    }
}
