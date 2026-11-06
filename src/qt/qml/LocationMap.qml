pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: picker
    required property real selectedLatitude
    required property real selectedLongitude
    readonly property bool validSelection: isFinite(selectedLatitude) && isFinite(selectedLongitude)
        && selectedLatitude >= -90 && selectedLatitude <= 90
        && selectedLongitude >= -180 && selectedLongitude <= 180
    signal locationSelected(real latitude, real longitude)
    spacing: 4

    Label {
        id: instruction
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: map.status === Image.Error
            ? qsTr("Map unavailable. Enter coordinates below.")
            : qsTr("Click the map to choose a location, or use the arrow keys.")
    }
    Image {
        id: map
        objectName: "locationMapImage"
        Layout.fillWidth: true
        Layout.preferredHeight: width / 2
        source: visible ? "world-map.png" : ""
        sourceSize.width: 1440
        sourceSize.height: 720
        enabled: status === Image.Ready
        activeFocusOnTab: true
        Accessible.role: Accessible.Graphic
        Accessible.name: qsTr("Location map")
        Accessible.description: instruction.text

        Keys.onPressed: event => {
            let lat = picker.validSelection ? picker.selectedLatitude : 0
            let lon = picker.validSelection ? picker.selectedLongitude : 0
            switch (event.key) {
            case Qt.Key_Left: lon -= 1; break
            case Qt.Key_Right: lon += 1; break
            case Qt.Key_Up: lat += 1; break
            case Qt.Key_Down: lat -= 1; break
            default: event.accepted = false; return
            }
            event.accepted = true
            picker.locationSelected(Math.max(-90, Math.min(90, lat)), Math.max(-180, Math.min(180, lon)))
        }

        Rectangle {
            id: marker
            objectName: "locationMapMarker"
            visible: picker.validSelection
            width: 12
            height: 12
            radius: 6
            color: "#b71c1c"
            border.color: "#ffffff"
            border.width: 2
            x: picker.validSelection
                ? Math.max(0, Math.min(map.width - width, (picker.selectedLongitude + 180) / 360 * map.width - width / 2)) : 0
            y: picker.validSelection
                ? Math.max(0, Math.min(map.height - height, (90 - picker.selectedLatitude) / 180 * map.height - height / 2)) : 0
            Accessible.ignored: true
        }
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: map.activeFocus ? 3 : 1
            border.color: map.activeFocus ? instruction.palette.highlight : "#657e81"
            Accessible.ignored: true
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.CrossCursor
            onClicked: mouse => {
                map.forceActiveFocus()
                // The bundled map is equirectangular, covering the full world without padding.
                picker.locationSelected(90 - mouse.y / height * 180, mouse.x / width * 360 - 180)
            }
        }
    }
}
