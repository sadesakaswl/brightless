pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var controller
    spacing: 12
    readonly property bool use12HourTime: controller.use_12_hour_time
    readonly property bool compact: width < 480

    function formattedTime(time) {
        if (!use12HourTime) return time
        const hour = Number(time.slice(0, 2))
        const displayHour = hour % 12 || 12
        return (displayHour < 10 ? "0" : "") + displayHour + time.slice(2)
            + (hour < 12 ? " AM" : " PM")
    }

    function canonicalTime(text) {
        if (!use12HourTime) return text
        const hour = Number(text.slice(0, 2)) % 12 + (text.slice(6).toUpperCase() === "PM" ? 12 : 0)
        return (hour < 10 ? "0" : "") + hour + text.slice(2, 5)
    }

    readonly property var triggers: [
        { text: qsTr("Daily time"), key: "time" },
        { text: qsTr("Battery percentage"), key: "battery" }
    ]
    readonly property var batteryConditions: [
        { text: qsTr("At or below"), key: "below" },
        { text: qsTr("At or above"), key: "above" }
    ]

    readonly property var controls: {
        controller.revision
        return [
            { text: controller.dynamic_contrast_enabled() ? qsTr("Dynamic Contrast") : qsTr("Brightness"), code: 0x10 },
            { text: qsTr("Contrast"), code: 0x12 },
            { text: qsTr("Volume"), code: 0x62 },
            { text: qsTr("Power mode"), code: 0xd6 },
            { text: qsTr("Input source"), code: 0x60 }
        ]
    }
    readonly property var inputChoices: [
        { text: "VGA", code: 1 }, { text: "DVI", code: 3 },
        { text: "DisplayPort 1", code: 15 }, { text: "DisplayPort 2", code: 16 },
        { text: "HDMI 1", code: 17 }, { text: "HDMI 2", code: 18 },
        { text: "HDMI 3", code: 19 }, { text: "HDMI 4", code: 20 },
        { text: "USB-C", code: 27 }
    ]
    readonly property var powerChoices: [
        { text: qsTr("On"), code: 1 }, { text: qsTr("Standby"), code: 2 },
        { text: qsTr("Suspend"), code: 3 }, { text: qsTr("Off"), code: 4 },
        { text: qsTr("Normal"), code: 5 }
    ]

    ScrollView {
        id: scroll
        objectName: "scheduleScroll"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 12

            Label {
                text: qsTr("Schedules run while Brightless is running. Daily times use local time; missed times are skipped.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("Brightness uses dynamic contrast on monitors where it is enabled.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("Battery entries run once when the condition becomes true, including at startup, and re-arm when it becomes false.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                text: page.controller.battery_percent >= 0
                    ? qsTr("Current battery: %1%").arg(page.controller.battery_percent)
                    : qsTr("Battery percentage unavailable. Battery entries will not run.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Time format"); Layout.fillWidth: true }
                ComboBox {
                    objectName: "timeFormatSelector"
                    Accessible.name: qsTr("Time format")
                    model: [qsTr("24-hour"), qsTr("12-hour (AM/PM)")]
                    currentIndex: page.use12HourTime ? 1 : 0
                    onActivated: page.controller.use_12_hour_time = currentIndex === 1
                }
            }

            Label {
                text: qsTr("No timing entries.")
                visible: page.controller.timings.length === 0
                Layout.fillWidth: true
            }

            Repeater {
                model: page.controller.timings.length
                ControlCard {
                    id: row
                    padding: 12
                    required property int index
                    // A removed entry can disappear before its delegate is destroyed.
                    readonly property var entry: page.controller.timings[index]
                        ?? { trigger: "time", time: "00:00", battery_percent: 20, battery_condition: "below",
                             monitor_id: "", control: 0x10, value: 50, enabled: false }
                    readonly property var triggerEditors: ({ time: timeEditor, battery: batteryEditor })
                    readonly property var monitors: {
                        const choices = [{ name: qsTr("All monitors"), id: "" }].concat(page.controller.ddc_monitors)
                        if (entry.monitor_id && !choices.some(monitor => monitor.id === entry.monitor_id))
                            choices.push({ name: qsTr("Unavailable monitor (%1)").arg(entry.monitor_id), id: entry.monitor_id })
                        return choices
                    }
                    readonly property var valueChoices: entry.control === 0xd6 ? page.powerChoices : page.inputChoices
                    Layout.fillWidth: true

                    function update(changes) {
                        page.controller.update_timing(index, Object.assign({}, entry, changes))
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        GridLayout {
                            Layout.fillWidth: true
                            columns: page.compact ? 2 : 4
                            rowSpacing: 8
                            Label {
                                text: qsTr("Trigger")
                                Layout.columnSpan: page.compact ? 2 : 1
                            }
                            ComboBox {
                                objectName: "triggerSelector"
                                Layout.columnSpan: page.compact ? 2 : 1
                                Layout.fillWidth: true
                                Accessible.name: qsTr("Trigger")
                                model: page.triggers
                                textRole: "text"
                                valueRole: "key"
                                currentIndex: page.triggers.findIndex(trigger => trigger.key === (row.entry.trigger ?? "time"))
                                onActivated: row.update({ trigger: currentValue,
                                    battery_percent: row.entry.battery_percent ?? 20,
                                    battery_condition: row.entry.battery_condition ?? "below" })
                            }
                            Switch {
                                text: qsTr("Enabled")
                                checked: row.entry.enabled
                                onToggled: row.update({ enabled: checked })
                            }
                            ToolButton {
                                Layout.alignment: Qt.AlignRight
                                text: "−"
                                Accessible.name: qsTr("Remove entry")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Remove entry")
                                onClicked: page.controller.remove_timing(row.index)
                            }
                        }

                        Loader {
                            Layout.fillWidth: true
                            sourceComponent: row.triggerEditors[row.entry.trigger ?? "time"]
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            Accessible.name: qsTr("Monitor")
                            model: row.monitors
                            textRole: "name"
                            valueRole: "id"
                            currentIndex: row.monitors.findIndex(monitor => monitor.id === row.entry.monitor_id)
                            onActivated: row.update({ monitor_id: currentValue })
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            ComboBox {
                                Layout.fillWidth: true
                                Accessible.name: qsTr("Control")
                                model: page.controls
                                textRole: "text"
                                valueRole: "code"
                                currentIndex: page.controls.findIndex(control => control.code === row.entry.control)
                                onActivated: row.update({ control: currentValue,
                                    value: currentValue === 0xd6 || currentValue === 0x60 ? 1 : 50 })
                            }
                            SpinBox {
                                visible: row.entry.control !== 0xd6 && row.entry.control !== 0x60
                                Accessible.name: qsTr("Value (%)")
                                from: 0
                                to: 100
                                editable: true
                                value: row.entry.value
                                onValueModified: row.update({ value: value })
                            }
                            Label {
                                text: "%"
                                visible: row.entry.control !== 0xd6 && row.entry.control !== 0x60
                            }
                            ComboBox {
                                visible: row.entry.control === 0xd6 || row.entry.control === 0x60
                                Layout.preferredWidth: 150
                                Accessible.name: qsTr("Value")
                                model: row.valueChoices
                                textRole: "text"
                                valueRole: "code"
                                currentIndex: row.valueChoices.findIndex(choice => choice.code === row.entry.value)
                                onActivated: row.update({ value: currentValue })
                            }
                        }
                    }

                    Component {
                        id: timeEditor
                        RowLayout {
                            Label {
                                text: page.use12HourTime ? qsTr("Time (hh:mm AM/PM)") : qsTr("Time (HH:mm)")
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            TextField {
                                objectName: "scheduleTime"
                                Layout.preferredWidth: page.use12HourTime ? 140 : 80
                                Accessible.name: page.use12HourTime ? qsTr("Time (hh:mm AM/PM)") : qsTr("Time (HH:mm)")
                                text: page.formattedTime(row.entry.time)
                                maximumLength: page.use12HourTime ? 8 : 5
                                selectByMouse: true
                                validator: RegularExpressionValidator {
                                    regularExpression: page.use12HourTime
                                        ? /^(?:0[1-9]|1[0-2]):[0-5][0-9] [AP]M$/i
                                        : /^(?:[01][0-9]|2[0-3]):[0-5][0-9]$/
                                }
                                onActiveFocusChanged: {
                                    if (!activeFocus) {
                                        if (acceptableInput) {
                                            const time = page.canonicalTime(text)
                                            if (time !== row.entry.time)
                                                row.update({ time: time })
                                        }
                                        text = Qt.binding(() => page.formattedTime(row.entry.time))
                                    }
                                }
                                onAccepted: focus = false
                            }
                        }
                    }
                    Component {
                        id: batteryEditor
                        RowLayout {
                            ComboBox {
                                objectName: "batteryCondition"
                                Layout.fillWidth: true
                                Accessible.name: qsTr("Battery condition")
                                model: page.batteryConditions
                                textRole: "text"
                                valueRole: "key"
                                currentIndex: page.batteryConditions.findIndex(condition => condition.key === row.entry.battery_condition)
                                onActivated: row.update({ battery_condition: currentValue })
                            }
                            SpinBox {
                                objectName: "batteryThreshold"
                                Accessible.name: qsTr("Battery threshold (%)")
                                from: 0
                                to: 100
                                editable: true
                                value: row.entry.battery_percent
                                onValueModified: row.update({ battery_percent: value })
                            }
                            Label { text: "%" }
                        }
                    }
                }
            }
        }
    }

    Button {
        text: qsTr("Add entry")
        onClicked: page.controller.add_timing()
    }
}
