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
            text: panel.controller.available
                ? qsTr("Uses Plasma Night Light. Plasma keeps the schedule running after Brightless closes.")
                : qsTr("Night Light requires KDE Plasma with a compatible KWin compositor.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
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
                text: qsTr("Enable Night Light")
                checked: panel.saved.enabled
            }
            Label { text: qsTr("Schedule") }
            ComboBox {
                id: mode
                objectName: "nightLightMode"
                Layout.fillWidth: true
                Accessible.name: qsTr("Schedule")
                model: [qsTr("Sunset to sunrise"), qsTr("Custom times"), qsTr("Always on")]
                currentIndex: panel.saved.mode
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
                    objectName: "detectNightLightLocation"
                    text: qsTr("Detect location via ipwho.is")
                    onClicked: panel.controller.detectLocation()
                }
                Label {
                    text: qsTr("ipwho.is receives your public IP address to estimate your location. Plasma settings change only when you apply.")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
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
            Label {
                visible: panel.controller.sharedSchedule && mode.currentIndex !== 2
                text: qsTr("These timings are shared with Plasma's other day/night features.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
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
