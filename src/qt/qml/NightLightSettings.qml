pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GroupBox {
    id: panel
    required property var controller
    title: qsTr("Night Light")
    property var saved: controller.settings

    Component.onCompleted: controller.initialize()
    Connections {
        target: panel.controller
        function onLocationDetected(lat, lon) {
            automaticLocation.checked = false
            latitude.text = lat.toLocaleString(Qt.locale(), 'f', 6)
            longitude.text = lon.toLocaleString(Qt.locale(), 'f', 6)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            objectName: "nightLightUnavailable"
            visible: !panel.controller.available && !panel.controller.busy
            text: qsTr("Night Light requires KDE Plasma with a compatible KWin compositor.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "nightLightBusy"
            visible: panel.controller.busy
            text: qsTr("Updating Night Light…")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ColumnLayout {
            visible: panel.controller.available
            enabled: !panel.controller.busy
            Layout.fillWidth: true
            spacing: 8

            Switch {
                id: active
                objectName: "nightLightEnabled"
                hoverEnabled: true
                text: qsTr("Enable Night Light")
                checked: panel.saved.enabled
                Accessible.description: nightLightHelp.text
                ToolTip {
                    id: nightLightHelp
                    objectName: "nightLightHelp"
                    visible: active.visible && (active.hovered || active.activeFocus)
                    delay: 500
                    implicitWidth: Math.min(320, panel.width)
                    text: qsTr("Uses Plasma Night Light. Plasma keeps the schedule running after Brightless closes.")
                }
            }
            Label { text: qsTr("Schedule") }
            ComboBox {
                id: mode
                objectName: "nightLightMode"
                hoverEnabled: true
                Layout.fillWidth: true
                Accessible.name: qsTr("Schedule")
                Accessible.description: modeHelp.text
                model: [qsTr("Sunset to sunrise"), qsTr("Custom times"), qsTr("Always on")]
                currentIndex: panel.saved.mode
                ToolTip {
                    id: modeHelp
                    objectName: "nightLightScheduleHelp"
                    visible: text.length > 0 && mode.visible && (mode.hovered || mode.activeFocus)
                    delay: 500
                    implicitWidth: Math.min(320, panel.width)
                    text: panel.controller.sharedSchedule && mode.currentIndex !== 2
                        ? qsTr("These timings are shared with Plasma's other day/night features.") : ""
                }
            }
            ColumnLayout {
                visible: mode.currentIndex === 0
                Layout.fillWidth: true

                CheckBox {
                    id: automaticLocation
                    objectName: "nightLightAutomaticLocation"
                    text: qsTr("Use Plasma's automatic location")
                    checked: panel.saved.automaticLocation
                }
                Button {
                    id: detectLocation
                    objectName: "detectNightLightLocation"
                    hoverEnabled: true
                    text: qsTr("Detect location via ipwho.is")
                    Accessible.description: locationHelp.text
                    onClicked: panel.controller.detectLocation()
                    ToolTip {
                        id: locationHelp
                        objectName: "nightLightLocationHelp"
                        visible: detectLocation.visible && (detectLocation.hovered || detectLocation.activeFocus)
                        delay: 500
                        implicitWidth: Math.min(320, panel.width)
                        text: qsTr("ipwho.is receives your public IP address to estimate your location. Plasma settings change only when you apply.")
                    }
                }
                LocationMap {
                    objectName: "nightLightLocationMap"
                    visible: !automaticLocation.checked
                    Layout.fillWidth: true
                    selectedLatitude: latitude.acceptableInput ? Number.fromLocaleString(Qt.locale(), latitude.text) : NaN
                    selectedLongitude: longitude.acceptableInput ? Number.fromLocaleString(Qt.locale(), longitude.text) : NaN
                    onLocationSelected: (lat, lon) => {
                        latitude.text = lat.toLocaleString(Qt.locale(), 'f', 6)
                        longitude.text = lon.toLocaleString(Qt.locale(), 'f', 6)
                    }
                }
                GridLayout {
                    enabled: !automaticLocation.checked
                    columns: 2
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Latitude (-90 to 90)")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: latitude
                        objectName: "nightLightLatitude"
                        Accessible.name: qsTr("Latitude (-90 to 90)")
                        Layout.preferredWidth: 140
                        text: Number(panel.saved.latitude).toLocaleString(Qt.locale(), 'f', 6)
                        selectByMouse: true
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        validator: DoubleValidator {
                            bottom: -90; top: 90; decimals: 6
                            notation: DoubleValidator.StandardNotation
                            locale: Qt.locale().name
                        }
                    }
                    Label {
                        text: qsTr("Longitude (-180 to 180)")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: longitude
                        objectName: "nightLightLongitude"
                        Accessible.name: qsTr("Longitude (-180 to 180)")
                        Layout.preferredWidth: 140
                        text: Number(panel.saved.longitude).toLocaleString(Qt.locale(), 'f', 6)
                        selectByMouse: true
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        validator: DoubleValidator {
                            bottom: -180; top: 180; decimals: 6
                            notation: DoubleValidator.StandardNotation
                            locale: Qt.locale().name
                        }
                    }
                }
            }
            Label { text: qsTr("Night temperature: %1 K").arg(Math.round(temperature.value)) }
            Slider {
                id: temperature
                objectName: "nightLightTemperature"
                Layout.fillWidth: true
                Accessible.name: qsTr("Night temperature")
                from: 1000
                to: 6500
                stepSize: 100
                value: panel.saved.temperature
            }
            GridLayout {
                visible: mode.currentIndex === 1
                columns: 2
                Layout.fillWidth: true
                Label {
                    text: qsTr("Night starts (24-hour)")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                TextField {
                    id: evening
                    objectName: "nightLightEvening"
                    Accessible.name: qsTr("Night starts (24-hour)")
                    Layout.preferredWidth: 100
                    text: panel.saved.evening
                    selectByMouse: true
                    validator: RegularExpressionValidator { regularExpression: /([01][0-9]|2[0-3]):[0-5][0-9]/ }
                }
                Label {
                    text: qsTr("Day starts (24-hour)")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                TextField {
                    id: morning
                    objectName: "nightLightMorning"
                    Accessible.name: qsTr("Day starts (24-hour)")
                    Layout.preferredWidth: 100
                    text: panel.saved.morning
                    selectByMouse: true
                    validator: RegularExpressionValidator { regularExpression: /([01][0-9]|2[0-3]):[0-5][0-9]/ }
                }
                Label {
                    text: qsTr("Smooth transition (minutes)")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                SpinBox {
                    id: transition
                    objectName: "nightLightTransition"
                    Accessible.name: qsTr("Smooth transition (minutes)")
                    from: 1
                    to: 720
                    editable: true
                    value: panel.saved.transition
                }
            }
            Button {
                objectName: "applyNightLight"
                text: qsTr("Apply Night Light")
                enabled: (mode.currentIndex !== 1 || (morning.acceptableInput && evening.acceptableInput))
                    && (mode.currentIndex !== 0 || automaticLocation.checked
                        || (latitude.acceptableInput && longitude.acceptableInput))
                onClicked: panel.controller.apply({
                    enabled: active.checked, mode: mode.currentIndex,
                    temperature: Math.round(temperature.value),
                    evening: evening.text, morning: morning.text, transition: transition.value,
                    automaticLocation: automaticLocation.checked,
                    latitude: latitude.acceptableInput ? Number.fromLocaleString(Qt.locale(), latitude.text) : panel.saved.latitude,
                    longitude: longitude.acceptableInput ? Number.fromLocaleString(Qt.locale(), longitude.text) : panel.saved.longitude
                })
            }
        }
        Label {
            objectName: "nightLightError"
            visible: text.length > 0
            text: panel.controller.error
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
