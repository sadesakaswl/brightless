import QtQuick
import QtTest
import "../src/qt/qml" as Ui

Item {
    id: root
    width: 360
    height: 900

    Component { id: backendComponent; FakeNightLight {} }
    Component {
        id: panelComponent
        Ui.NightLightSettings { width: root.width }
    }

    TestCase {
        name: "NightLightSettings"
        when: windowShown
        property var backend
        property var panel

        function init() {
            failOnWarning(/.*/)
            backend = createTemporaryObject(backendComponent, root)
            panel = createTemporaryObject(panelComponent, root, { controller: backend })
            verify(panel !== null)
        }

        function test_apply_custom_schedule() {
            const active = findChild(panel, "nightLightEnabled")
            const temperature = findChild(panel, "nightLightTemperature")
            const dayTemperature = findChild(panel, "nightLightDayTemperature")
            verify(!!dayTemperature, "Object exists")
            const evening = findChild(panel, "nightLightEvening")
            const morning = findChild(panel, "nightLightMorning")
            const transition = findChild(panel, "nightLightTransition")
            mouseClick(active)
            temperature.value = 3500
            dayTemperature.value = 5500
            evening.text = "21:30"
            morning.text = "07:15"
            transition.value = 45
            compare(backend.lastApplied, null)
            mouseClick(findChild(panel, "applyNightLight"))
            compare(backend.lastApplied.enabled, true)
            compare(backend.lastApplied.mode, 1)
            compare(backend.lastApplied.temperature, 3500)
            compare(backend.lastApplied.dayTemperature, 5500)
            compare(backend.lastApplied.evening, "21:30")
            compare(backend.lastApplied.morning, "07:15")
            compare(backend.lastApplied.transition, 45)
            verify(panel.implicitHeight <= 640)
            verify(transition.width > 0 && transition.x + transition.width <= panel.width)
        }

        function test_morning_temperature_draft_and_modes() {
            const temperature = findChild(panel, "nightLightDayTemperature")
            verify(!!temperature, "Object exists")
            const mode = findChild(panel, "nightLightMode")
            verify(!!mode, "Object exists")
            const apply = findChild(panel, "applyNightLight")
            verify(!!apply, "Object exists")
            const before = JSON.stringify(backend.settings)
            compare(temperature.from, 1000)
            compare(temperature.to, 6500)
            compare(temperature.stepSize, 100)
            compare(temperature.value, 6500)
            verify(temperature.visible)
            temperature.forceActiveFocus()
            keyClick(Qt.Key_Left)
            compare(temperature.value, 6400)
            compare(JSON.stringify(backend.settings), before)
            compare(backend.lastApplied, null)
            mouseClick(apply)
            tryVerify(() => backend.lastApplied !== null)
            compare(backend.lastApplied.dayTemperature, 6400)
            compare(backend.lastApplied.temperature, 4500)
            mode.currentIndex = 0
            verify(temperature.visible)
            mode.currentIndex = 2
            verify(!temperature.visible)
            mouseClick(apply)
            tryCompare(backend.lastApplied, "mode", 2)
            compare(backend.lastApplied.dayTemperature, 6400)
            mode.currentIndex = 1
            verify(temperature.visible)
            backend.busy = true
            verify(!temperature.enabled)
            backend.busy = false
            backend.available = false
            verify(!temperature.visible)
            compare(backend.lastApplied.dayTemperature, 6400)
        }

        function test_modes_validation_and_availability() {
            const mode = findChild(panel, "nightLightMode")
            const evening = findChild(panel, "nightLightEvening")
            const transition = findChild(panel, "nightLightTransition")
            const apply = findChild(panel, "applyNightLight")
            evening.text = "25:00"
            verify(!apply.enabled)
            evening.text = "20:00"
            verify(apply.enabled)
            mode.currentIndex = 0
            verify(!transition.visible)
            mode.currentIndex = 2
            mouseClick(apply)
            compare(backend.lastApplied.mode, 2)
            backend.busy = true
            verify(!apply.enabled)
            backend.busy = false
            backend.available = false
            verify(!apply.visible)
            backend.error = "Could not save"
            compare(findChild(panel, "nightLightError").text, "Could not save")
            compare(findChild(panel, "nightLightError").textFormat, Text.PlainText)
        }

        function test_help_tooltips() {
            const state = JSON.stringify(backend.settings)
            const active = findChild(panel, "nightLightEnabled")
            const help = findChild(panel, "nightLightHelp")
            panel.forceActiveFocus()
            mouseMove(root, root.width - 1, root.height - 1)
            verify(!help.visible)
            mouseMove(active, active.width / 2, active.height / 2)
            tryCompare(active, "hovered", true)
            tryCompare(help, "visible", true, 1500)
            verify(help.width <= 320 && help.width <= panel.width)
            compare(active.Accessible.description, help.text)
            mouseMove(root, root.width - 1, root.height - 1)
            tryCompare(help, "visible", false)

            backend.sharedSchedule = true
            const mode = findChild(panel, "nightLightMode")
            const modeHelp = findChild(panel, "nightLightScheduleHelp")
            mode.forceActiveFocus()
            tryCompare(modeHelp, "visible", true, 1500)
            verify(modeHelp.text.length > 0)
            compare(mode.Accessible.description, modeHelp.text)
            mode.currentIndex = 2
            tryCompare(modeHelp, "visible", false)
            mode.currentIndex = 0
            const detect = findChild(panel, "detectNightLightLocation")
            const locationHelp = findChild(panel, "nightLightLocationHelp")
            detect.forceActiveFocus()
            tryCompare(locationHelp, "visible", true, 1500)
            verify(locationHelp.text.includes("public IP"))
            compare(detect.Accessible.description, locationHelp.text)
            compare(backend.locationRequests, 0)
            compare(JSON.stringify(backend.settings), state)

            backend.available = false
            tryCompare(locationHelp, "visible", false)
            verify(findChild(panel, "nightLightUnavailable").visible)
            backend.busy = true
            verify(findChild(panel, "nightLightBusy").visible)
            verify(!findChild(panel, "nightLightUnavailable").visible)
            backend.error = "Unavailable"
            verify(findChild(panel, "nightLightError").visible)
        }

        function test_ip_location_detection() {
            findChild(panel, "nightLightMode").currentIndex = 0
            const detect = findChild(panel, "detectNightLightLocation")
            const before = JSON.stringify(backend.settings)
            mouseClick(detect)
            compare(backend.locationRequests, 1)
            backend.busy = true
            verify(!detect.enabled)
            backend.busy = false
            backend.locationDetected(-33.8688, 151.2093)
            compare(findChild(panel, "nightLightAutomaticLocation").checked, false)
            compare(Number.fromLocaleString(Qt.locale(), findChild(panel, "nightLightLatitude").text), -33.8688)
            compare(Number.fromLocaleString(Qt.locale(), findChild(panel, "nightLightLongitude").text), 151.2093)
            compare(JSON.stringify(backend.settings), before)
            compare(backend.lastApplied, null)
            mouseClick(findChild(panel, "applyNightLight"))
            compare(backend.lastApplied.automaticLocation, false)
            compare(backend.lastApplied.latitude, -33.8688)
            compare(backend.lastApplied.longitude, 151.2093)
        }

        function test_manual_location() {
            const mode = findChild(panel, "nightLightMode")
            const automatic = findChild(panel, "nightLightAutomaticLocation")
            const latitude = findChild(panel, "nightLightLatitude")
            const longitude = findChild(panel, "nightLightLongitude")
            const apply = findChild(panel, "applyNightLight")
            verify(!automatic.visible)
            mode.currentIndex = 0
            verify(automatic.visible)
            verify(automatic.checked)
            verify(!latitude.enabled && !longitude.enabled)
            mouseClick(automatic)
            verify(latitude.enabled && longitude.enabled)
            latitude.text = (-33.8688).toLocaleString(Qt.locale(), 'f', 6)
            longitude.text = (151.2093).toLocaleString(Qt.locale(), 'f', 6)
            verify(apply.enabled)
            compare(backend.lastApplied, null)
            mouseClick(apply)
            compare(backend.lastApplied.mode, 0)
            compare(backend.lastApplied.automaticLocation, false)
            compare(backend.lastApplied.latitude, -33.8688)
            compare(backend.lastApplied.longitude, 151.2093)

            for (const value of ["", "-", "bad", "1" + Qt.locale().groupSeparator + "0",
                                 (90.1).toLocaleString(Qt.locale())]) {
                latitude.text = value
                verify(!apply.enabled)
            }
            latitude.text = (-90).toLocaleString(Qt.locale())
            longitude.text = (180.1).toLocaleString(Qt.locale())
            verify(!apply.enabled)
            longitude.text = (180).toLocaleString(Qt.locale())
            verify(apply.enabled)
            longitude.text = "bad"
            mouseClick(automatic)
            verify(apply.enabled)
            mouseClick(apply)
            compare(backend.lastApplied.automaticLocation, true)
            compare(backend.lastApplied.longitude, 151.2093)
            mode.currentIndex = 2
            verify(!automatic.visible)
        }

        function test_map_selection_is_a_manual_draft() {
            const map = findChild(panel, "nightLightLocationMap")
            verify(!!map, "Object exists")
            const image = findChild(map, "locationMapImage")
            verify(!!image, "Object exists")
            const marker = findChild(map, "locationMapMarker")
            verify(!!marker, "Object exists")
            const automatic = findChild(panel, "nightLightAutomaticLocation")
            verify(!!automatic, "Object exists")
            const mode = findChild(panel, "nightLightMode")
            verify(!!mode, "Object exists")
            const latitude = findChild(panel, "nightLightLatitude")
            verify(!!latitude, "Object exists")
            const longitude = findChild(panel, "nightLightLongitude")
            verify(!!longitude, "Object exists")
            const apply = findChild(panel, "applyNightLight")
            verify(!!apply, "Object exists")
            const before = JSON.stringify(backend.settings)
            verify(!map.visible)
            mode.currentIndex = 0
            verify(!map.visible)
            mouseClick(automatic)
            tryCompare(map, "visible", true)
            tryCompare(image, "status", Image.Ready)
            verify(waitForRendering(panel))
            verify(image.width > 0 && image.height > 0 && image.width <= panel.width)
            const x = Math.floor(image.width * 0.75)
            const y = Math.floor(image.height * 0.75)
            const lat = Number((90 - y / image.height * 180).toFixed(6))
            const lon = Number((x / image.width * 360 - 180).toFixed(6))
            mouseClick(image, x, y)
            tryCompare(map, "selectedLatitude", lat)
            tryCompare(map, "selectedLongitude", lon)
            compare(Number.fromLocaleString(Qt.locale(), latitude.text), lat)
            compare(Number.fromLocaleString(Qt.locale(), longitude.text), lon)
            compare(JSON.stringify(backend.settings), before)
            compare(backend.lastApplied, null)
            compare(backend.locationRequests, 0)
            mouseClick(apply)
            tryVerify(() => backend.lastApplied !== null)
            compare(backend.lastApplied.latitude, lat)
            compare(backend.lastApplied.longitude, lon)
            compare(backend.lastApplied.automaticLocation, false)

            latitude.text = "-"
            verify(!marker.visible && !apply.enabled)
            latitude.text = (51.5).toLocaleString(Qt.locale(), 'f', 6)
            longitude.text = (-0.125).toLocaleString(Qt.locale(), 'f', 6)
            verify(marker.visible)
            compare(map.selectedLatitude, 51.5)
            compare(map.selectedLongitude, -0.125)
            backend.locationDetected(-33.8688, 151.2093)
            compare(map.selectedLatitude, -33.8688)
            compare(map.selectedLongitude, 151.2093)
            backend.busy = true
            verify(!map.enabled)
            backend.busy = false
            automatic.checked = true
            verify(!map.visible)
            automatic.checked = false
            mode.currentIndex = 1
            verify(!map.visible)
            mode.currentIndex = 2
            verify(!map.visible)
            mode.currentIndex = 0
            backend.available = false
            verify(!map.visible)
            compare(backend.locationRequests, 0)
        }

        function test_load_plasma_settings() {
            backend.settings = { enabled: true, mode: 0, temperature: 3000, dayTemperature: 6100,
                                 evening: "22:00", morning: "05:30", transition: 60,
                                 automaticLocation: false, latitude: 51.5, longitude: -0.125 }
            compare(findChild(panel, "nightLightEnabled").checked, true)
            compare(findChild(panel, "nightLightTemperature").value, 3000)
            compare(findChild(panel, "nightLightDayTemperature").value, 6100)
            compare(findChild(panel, "nightLightEvening").text, "22:00")
            compare(findChild(panel, "nightLightMorning").text, "05:30")
            compare(findChild(panel, "nightLightTransition").value, 60)
            compare(findChild(panel, "nightLightAutomaticLocation").checked, false)
            const latitude = findChild(panel, "nightLightLatitude")
            const longitude = findChild(panel, "nightLightLongitude")
            compare(Number.fromLocaleString(Qt.locale(), latitude.text), 51.5)
            compare(Number.fromLocaleString(Qt.locale(), longitude.text), -0.125)
            verify(latitude.enabled && longitude.enabled)
        }
    }
}
