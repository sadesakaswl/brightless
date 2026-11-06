import QtQuick
import QtTest
import "../src/qt/qml" as Ui

Item {
    id: root
    width: 480
    height: 360

    Component {
        id: mapComponent
        Ui.LocationMap {
            width: 400
            selectedLatitude: 0
            selectedLongitude: 0
            onLocationSelected: (lat, lon) => {
                selectedLatitude = lat
                selectedLongitude = lon
            }
        }
    }
    SignalSpy { id: selectedSpy; signalName: "locationSelected" }

    TestCase {
        name: "LocationMap"
        when: windowShown
        property var picker
        property var image
        property var marker

        function init() {
            failOnWarning(/.*/)
            picker = createTemporaryObject(mapComponent, root)
            verify(!!picker, "Component exists")
            image = findChild(picker, "locationMapImage")
            verify(!!image, "Object exists")
            marker = findChild(picker, "locationMapMarker")
            verify(!!marker, "Object exists")
            tryCompare(image, "status", Image.Ready)
            tryCompare(image, "height", 200)
            selectedSpy.target = picker
            selectedSpy.clear()
        }

        function test_locationSelected_data() {
            return [
                { tag: "zero", x: 200, y: 100, lat: 0, lon: 0 },
                { tag: "north-west", x: 100, y: 50, lat: 45, lon: -90 },
                { tag: "south-east", x: 300, y: 150, lat: -45, lon: 90 },
                { tag: "north-pole-antimeridian", x: 0, y: 0, lat: 90, lon: -180 }
            ]
        }

        function test_locationSelected(data) {
            mouseClick(image, data.x, data.y)
            tryCompare(selectedSpy, "count", 1)
            compare(selectedSpy.signalArguments[0][0], data.lat)
            compare(selectedSpy.signalArguments[0][1], data.lon)
            tryCompare(picker, "selectedLatitude", data.lat)
            tryCompare(picker, "selectedLongitude", data.lon)
            verify(marker.visible)
            verify(marker.x >= 0 && marker.x + marker.width <= image.width)
            verify(marker.y >= 0 && marker.y + marker.height <= image.height)
        }

        function test_keyboard_and_limits() {
            image.forceActiveFocus()
            picker.selectedLatitude = 90
            picker.selectedLongitude = 180
            keyClick(Qt.Key_Up)
            keyClick(Qt.Key_Right)
            compare(picker.selectedLatitude, 90)
            compare(picker.selectedLongitude, 180)
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Left)
            compare(picker.selectedLatitude, 89)
            compare(picker.selectedLongitude, 179)
            picker.selectedLatitude = -90
            picker.selectedLongitude = -180
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Left)
            compare(picker.selectedLatitude, -90)
            compare(picker.selectedLongitude, -180)
            keyClick(Qt.Key_Up)
            keyClick(Qt.Key_Right)
            compare(picker.selectedLatitude, -89)
            compare(picker.selectedLongitude, -179)
            picker.selectedLatitude = NaN
            keyClick(Qt.Key_Right)
            compare(picker.selectedLatitude, 0)
            compare(picker.selectedLongitude, 1)
        }

        function test_marker_validation_and_resize() {
            for (const point of [[NaN, 0], [0, NaN], [91, 0], [0, -181], [Infinity, 0]]) {
                picker.selectedLatitude = point[0]
                picker.selectedLongitude = point[1]
                verify(!marker.visible)
            }
            picker.selectedLatitude = -45
            picker.selectedLongitude = 90
            verify(marker.visible)
            compare(marker.x + marker.width / 2, image.width * 0.75)
            compare(marker.y + marker.height / 2, image.height * 0.75)
            picker.width = 320
            tryCompare(image, "width", 320)
            tryCompare(image, "height", 160)
            compare(marker.x + marker.width / 2, 240)
            compare(marker.y + marker.height / 2, 120)
            mouseClick(image, 80, 40)
            tryCompare(picker, "selectedLatitude", 45)
            tryCompare(picker, "selectedLongitude", -90)
        }
    }
}
