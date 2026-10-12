import QtQuick
import QtTest
import "../src/qt/qml" as Ui

Item {
    id: root
    width: 536
    height: 560

    QtObject {
        id: backend
        property var timings: [
            { time: "08:30", monitor_id: "", control: 0x10, value: 50, enabled: false },
            { time: "08:30", monitor_id: "gone", control: 0x60, value: 17, enabled: true }
        ]
        property var ddc_monitors: [{ id: "monitor-0", name: "Test display" }]
        property int revision: 0
        property bool use_12_hour_time: false
        property bool dynamic: false
        function dynamic_contrast_enabled() { return dynamic }
        function update_timing(index, entry) {
            const copy = timings.slice()
            copy[index] = entry
            timings = copy
            return true
        }
        function remove_timing(index) {
            const copy = timings.slice()
            copy.splice(index, 1)
            timings = copy
        }
        function add_timing() {
            timings = timings.concat([{ time: "12:00", monitor_id: "", control: 0xd6, value: 4, enabled: false }])
        }
    }

    Ui.TimingPage {
        id: page
        anchors.fill: parent
        controller: backend
    }

    TestCase {
        name: "TimingPage"
        when: windowShown

        function findItem(item, predicate) {
            if (predicate(item)) return item
            for (const child of item.children || []) {
                const found = findItem(child, predicate)
                if (found) return found
            }
            return null
        }

        function test_edit_remove_and_add() {
            failOnWarning(/.*/)
            const firstRow = findItem(page, item => item.entry !== undefined)
            verify(firstRow !== null)
            firstRow.update({ value: 70, enabled: true })
            wait(0)
            compare(firstRow.entry.value, 70)
            verify(firstRow.entry.enabled)

            const field = findItem(firstRow, item => item.maximumLength === 5)
            verify(field !== null)
            field.forceActiveFocus()
            field.text = "23:45"
            field.focus = false
            wait(0)
            compare(backend.timings[0].time, "23:45")
            field.forceActiveFocus()
            field.text = "invalid"
            field.focus = false
            wait(0)
            compare(field.text, "23:45")
            compare(backend.timings[0].time, "23:45")

            const selector = findChild(page, "timeFormatSelector")
            verify(selector !== null)
            const schedules = JSON.stringify(backend.timings)
            selector.currentIndex = 1
            selector.activated(1)
            wait(0)
            verify(backend.use_12_hour_time)
            compare(field.text, "11:45 PM")
            compare(field.maximumLength, 8)
            compare(JSON.stringify(backend.timings), schedules)

            for (const [canonical, display] of [
                ["00:00", "12:00 AM"], ["12:00", "12:00 PM"],
                ["00:15", "12:15 AM"], ["12:15", "12:15 PM"],
                ["11:59", "11:59 AM"], ["23:59", "11:59 PM"],
                ["01:05", "01:05 AM"], ["13:05", "01:05 PM"]
            ]) {
                firstRow.update({ time: canonical })
                wait(0)
                compare(field.text, display)
                compare(page.canonicalTime(display), canonical)
            }
            field.forceActiveFocus()
            field.text = "03:07 pm"
            field.focus = false
            wait(0)
            compare(backend.timings[0].time, "15:07")
            compare(field.text, "03:07 PM")
            for (const invalid of ["00:30 AM", "13:00 PM", "12:60 AM", "11:30 XX", "11:30"]) {
                field.forceActiveFocus()
                field.text = invalid
                field.focus = false
                wait(0)
                compare(field.text, "03:07 PM")
                compare(backend.timings[0].time, "15:07")
            }
            const updatedSchedules = JSON.stringify(backend.timings)
            selector.currentIndex = 0
            selector.activated(0)
            wait(0)
            verify(!backend.use_12_hour_time)
            compare(field.text, "15:07")
            compare(field.maximumLength, 5)
            compare(JSON.stringify(backend.timings), updatedSchedules)

            const brightnessLabel = page.controls[0].text
            backend.dynamic = true
            backend.revision++
            wait(0)
            verify(page.controls[0].text !== brightnessLabel)

            backend.remove_timing(0)
            wait(0)
            const row = findItem(page, item => item.entry !== undefined)
            compare(row.entry.monitor_id, "gone")
            compare(row.monitors[row.monitors.length - 1].id, "gone")
            backend.remove_timing(0)
            wait(0)
            compare(backend.timings.length, 0)
            compare(findItem(page, item => item.entry !== undefined), null)
            backend.add_timing()
            wait(0)
            compare(findItem(page, item => item.entry !== undefined).entry.control, 0xd6)
        }
    }
}
