pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: settingsWindow
    required property var controller
    required property var appWindow
    required property var desktop
    readonly property var window: appWindow
    title: qsTr("Settings")
    visible: false
    flags: Qt.Dialog
    transientParent: window
    width: Math.min(600, Screen.desktopAvailableWidth)
    height: Math.min(640, Screen.desktopAvailableHeight)
    minimumWidth: Math.min(360, Screen.desktopAvailableWidth)
    minimumHeight: Math.min(320, Screen.desktopAvailableHeight)
    color: palette.window

    function open() {
        x = window.x + Math.round((window.width - width) / 2);
        y = window.y + Math.round((window.height - height) / 2);
        show();
        requestActivate();
    }

    onActiveChanged: {
        if (visible && !active)
            Qt.callLater(() => close());
    }

    Shortcut {
        sequence: "Esc"
        onActivated: settingsWindow.close()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Settings")
                font.weight: Font.Medium
                font.pointSize: settingsWindow.font.pointSize * 1.25
                Layout.fillWidth: true
            }
            ComboBox {
                id: settingsTabs
                objectName: "settingsCategory"
                Layout.preferredWidth: Math.min(240, settingsWindow.width / 2)
                Accessible.name: qsTr("Settings")
                textRole: "text"
                model: [
                    {
                        text: qsTr("Appearance"),
                        tip: qsTr("Choose which monitor controls and icons are shown.")
                    },
                    {
                        text: qsTr("Behaviour"),
                        tip: qsTr("Adjust scrolling, DDC timing, and dynamic contrast.")
                    },
                    {
                        text: qsTr("DDC"),
                        tip: qsTr("Configure VCP codes for monitor controls.")
                    },
                    {
                        text: qsTr("Schedules"),
                        tip: qsTr("Trigger monitor changes by time or battery percentage.")
                    },
                    {
                        text: qsTr("System"),
                        tip: qsTr("Configure startup and window-closing behaviour.")
                    },
                    {
                        text: qsTr("About"),
                        tip: qsTr("View version, source code, and license information.")
                    }
                ]
                ToolTip.visible: hovered || activeFocus
                ToolTip.text: model[currentIndex].tip
            }
        }

        Loader {
            id: settingsPage
            objectName: "settingsPage"
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: settingsWindow.visible
            asynchronous: true
            visible: status === Loader.Ready
            focus: true
            sourceComponent: [appearancePage, behaviourPage, ddcPage, timingPage, systemPage, aboutPage][settingsTabs.currentIndex]
        }
    }

    Component {
        id: appearancePage
        ScrollView {
            id: appearanceScroll
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: appearanceScroll.availableWidth
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Theme")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    ComboBox {
                        objectName: "themeSelector"
                        Accessible.name: qsTr("Theme")
                        model: [
                            { text: qsTr("Auto"), mode: "auto" },
                            { text: qsTr("Light"), mode: "light" },
                            { text: qsTr("Dark"), mode: "dark" }
                        ]
                        textRole: "text"
                        valueRole: "mode"
                        currentIndex: model.findIndex(theme => theme.mode === settingsWindow.controller.theme_mode)
                        onActivated: settingsWindow.controller.theme_mode = currentValue
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        id: hideBrightnessLabel
                        wrapMode: Text.WordWrap
                        text: window.refreshed(controller.dynamic_contrast_enabled()) ? qsTr("Hide Dynamic Contrast Option") : qsTr("Hide Brightness Option")
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: hideBrightnessLabel.text
                        ToolTip.visible: hovered
                        ToolTip.text: window.refreshed(controller.dynamic_contrast_enabled()) ? qsTr("Remove the dynamic contrast control from monitor cards.") : qsTr("Remove the brightness control from monitor cards.")
                        checked: controller.hide_brightness
                        onToggled: controller.hide_brightness = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !window.refreshed(controller.dynamic_contrast_enabled())
                    Label {
                        text: qsTr("Hide Contrast Option")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: qsTr("Hide Contrast Option")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove the contrast control from monitor cards.")
                        checked: controller.hide_contrast
                        onToggled: controller.hide_contrast = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Hide Volume Option")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: qsTr("Hide Volume Option")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove the volume control from monitor cards.")
                        checked: controller.hide_volume
                        onToggled: controller.hide_volume = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Hide Input Option")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: qsTr("Hide Input Option")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove the input-source control from monitor cards.")
                        checked: controller.hide_input
                        onToggled: controller.hide_input = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Hide Tray Icon")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: qsTr("Hide Tray Icon")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove Brightless from the system tray.")
                        checked: controller.hide_tray_icon
                        onToggled: controller.hide_tray_icon = checked
                    }
                }
            }
        }
    }

    Component {
        id: behaviourPage
        ScrollView {
            id: behaviourScroll
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: behaviourScroll.availableWidth
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Scroll Step:")
                        Layout.fillWidth: true
                    }
                    Label {
                        text: scrollStepSlider.value.toFixed(0) + "%"
                    }
                }
                Slider {
                    id: scrollStepSlider
                    objectName: "scrollStepSlider"
                    wheelEnabled: true
                    from: 1
                    to: 10
                    stepSize: 1
                    value: window.refreshed(controller.scroll_step())
                    Accessible.name: qsTr("Scroll Step")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Change monitor values by %1% for each mouse-wheel step.").arg(Math.round(value))
                    Layout.fillWidth: true
                    onMoved: controller.set_scroll_step(Math.round(value))
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        id: ddcDelayLabel
                        text: qsTr("Delay to send DDC signal")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Label {
                        text: Math.round(ddcDelaySlider.value) === 0 ? qsTr("Instant") : Math.round(ddcDelaySlider.value) + " ms"
                    }
                }
                Slider {
                    id: ddcDelaySlider
                    objectName: "ddcDelaySlider"
                    wheelEnabled: true
                    from: 0
                    to: 1500
                    stepSize: 50
                    value: window.refreshed(controller.ddc_delay())
                    Accessible.name: ddcDelayLabel.text
                    ToolTip.visible: hovered
                    ToolTip.text: Math.round(value) === 0 ? qsTr("Send monitor control updates immediately.") : qsTr("Wait %1 ms before sending monitor control updates.").arg(Math.round(value))
                    Layout.fillWidth: true
                    onMoved: controller.set_ddc_delay(Math.round(value))
                }

                Label {
                    text: qsTr("Dynamic Contrast")
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Enable Dynamic Contrast")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: qsTr("Enable Dynamic Contrast")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Adjust brightness and contrast together using a configurable ratio.")
                        checked: window.refreshed(controller.dynamic_contrast_enabled())
                        onToggled: controller.set_dynamic_contrast_enabled(checked)
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: window.refreshed(controller.dynamic_contrast_enabled())
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Apply to all monitors")
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        Switch {
                            Accessible.name: qsTr("Apply to all monitors")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Use dynamic contrast on every compatible monitor.")
                            checked: window.refreshed(controller.dynamic_contrast_global())
                            onToggled: controller.set_dynamic_contrast_global(checked)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: !window.refreshed(controller.dynamic_contrast_per_monitor_ratio())
                        Label {
                            text: qsTr("Contrast Ratio:")
                            Layout.preferredWidth: 120
                        }
                        Label {
                            text: ratioSlider.value.toFixed(1)
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                    Slider {
                        id: ratioSlider
                        objectName: "ratioSlider"
                        wheelEnabled: true
                        visible: !window.refreshed(controller.dynamic_contrast_per_monitor_ratio())
                        from: 0.1
                        to: 2.0
                        stepSize: 0.1
                        value: window.refreshed(controller.dynamic_contrast_ratio())
                        Accessible.name: qsTr("Contrast Ratio")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Set contrast to %1 times brightness.").arg(value.toFixed(1))
                        Layout.fillWidth: true
                        onMoved: controller.set_dynamic_contrast_ratio(value)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Per-monitor ratio")
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        Switch {
                            Accessible.name: qsTr("Per-monitor ratio")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Allow each monitor to use its own contrast ratio.")
                            checked: window.refreshed(controller.dynamic_contrast_per_monitor_ratio())
                            onToggled: controller.set_dynamic_contrast_per_monitor_ratio(checked)
                        }
                    }

                    Repeater {
                        model: controller.monitor_count
                        ColumnLayout {
                            id: ratioDelegate
                            required property int index
                            Layout.fillWidth: true
                            visible: window.refreshed(window.backend.dynamic_contrast_per_monitor_ratio()) && window.backend.supports_contrast(ratioDelegate.index)
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: qsTr("%1 Ratio:").arg(window.backend.monitor_names[ratioDelegate.index])
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: perMonitorRatio.value.toFixed(1)
                                }
                            }
                            Slider {
                                id: perMonitorRatio
                                objectName: "perMonitorRatio"
                                wheelEnabled: true
                                from: 0.1
                                to: 2.0
                                stepSize: 0.1
                                value: window.refreshed(window.backend.monitor_ratio(ratioDelegate.index))
                                Accessible.name: qsTr("%1 Ratio").arg(window.backend.monitor_names[ratioDelegate.index])
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Set this monitor's contrast to %1 times its brightness.").arg(value.toFixed(1))
                                Layout.fillWidth: true
                                onMoved: window.backend.set_monitor_ratio(ratioDelegate.index, value)
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: ddcPage
        ScrollView {
            id: ddcScroll
            readonly property bool perMonitor: window.refreshed(controller.vcp_per_monitor())
            property string selectedMonitorId: ""
            readonly property string scope: perMonitor ? (monitorSelector.currentValue ?? "") : ""
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: ddcScroll.availableWidth
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Per-monitor VCP codes")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: qsTr("Per-monitor VCP codes")
                        checked: ddcScroll.perMonitor
                        onToggled: {
                            controller.set_vcp_per_monitor(checked);
                            controller.initialize();
                        }
                    }
                }

                ComboBox {
                    id: monitorSelector
                    visible: ddcScroll.perMonitor
                    Layout.fillWidth: true
                    Accessible.name: qsTr("Monitor")
                    model: controller.ddc_monitors
                    textRole: "name"
                    valueRole: "id"
                    currentIndex: model.length ? Math.max(0, model.findIndex(monitor => monitor.id === ddcScroll.selectedMonitorId)) : -1
                    onActivated: ddcScroll.selectedMonitorId = currentValue
                }

                Label {
                    text: ddcScroll.perMonitor ? qsTr("VCP codes (hexadecimal). Leave empty to use the grey default. Changes apply to the selected monitor.") : qsTr("VCP codes (hexadecimal). Leave empty to use the grey default. Changes apply to all monitors.")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Repeater {
                    model: [
                        {
                            label: qsTr("Brightness"),
                            code: 0x10
                        },
                        {
                            label: qsTr("Contrast"),
                            code: 0x12
                        },
                        {
                            label: qsTr("Volume"),
                            code: 0x62
                        },
                        {
                            label: qsTr("Input source"),
                            code: 0x60
                        },
                        {
                            label: qsTr("Power mode"),
                            code: 0xd6
                        }
                    ]
                    RowLayout {
                        id: vcpRow
                        required property var modelData
                        Layout.fillWidth: true
                        enabled: !controller.loading && (!ddcScroll.perMonitor || ddcScroll.scope.length > 0)
                        Label {
                            text: vcpRow.modelData.label
                            Layout.fillWidth: true
                        }
                        TextField {
                            Layout.preferredWidth: 100
                            Accessible.name: vcpRow.modelData.label
                            placeholderText: "0x" + vcpRow.modelData.code.toString(16).toUpperCase()
                            placeholderTextColor: palette.placeholderText
                            readonly property string configuredText: {
                                const code = window.refreshed(controller.vcp_code(vcpRow.modelData.code, ddcScroll.scope));
                                return code === vcpRow.modelData.code ? "" : "0x" + code.toString(16).toUpperCase();
                            }
                            text: configuredText
                            validator: RegularExpressionValidator {
                                regularExpression: /^(?:0[xX])?[0-9a-fA-F]{1,2}$|^$/
                            }
                            onEditingFinished: {
                                if (!enabled)
                                    return;
                                const code = text.length ? parseInt(text, 16) : vcpRow.modelData.code;
                                if (controller.set_vcp_code(vcpRow.modelData.code, code, ddcScroll.scope))
                                    controller.initialize();
                                text = Qt.binding(() => configuredText);
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: timingPage
        TimingPage {
            controller: settingsWindow.controller
        }
    }

    Component {
        id: systemPage
        ScrollView {
            id: systemScroll
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: systemScroll.availableWidth
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Autostart on login")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: qsTr("Autostart on login")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Launch Brightless automatically when you log in.")
                        checked: controller.autostart
                        onToggled: controller.autostart = checked
                    }
                }

                RowLayout {
                    id: autostartTrayRow
                    readonly property bool optionEnabled: controller.autostart && !controller.hide_tray_icon
                    Layout.fillWidth: true
                    ToolTip.visible: autostartTrayHover.hovered
                    ToolTip.text: qsTr("Start Brightless on login without opening its window.")
                    HoverHandler {
                        id: autostartTrayHover
                    }
                    Label {
                        text: qsTr("Autostart as tray icon")
                        enabled: autostartTrayRow.optionEnabled
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: qsTr("Autostart as tray icon")
                        enabled: autostartTrayRow.optionEnabled
                        checked: controller.autostart_as_tray_icon
                        onToggled: controller.autostart_as_tray_icon = checked
                    }
                }

                Button {
                    text: qsTr("Configure global shortcuts")
                    visible: settingsWindow.desktop.shortcutEditorAvailable
                    onClicked: settingsWindow.desktop.configureShortcuts()
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: settingsWindow.desktop.plasmaShortcutsAvailable
                    Label {
                        text: qsTr("Enable Plasma global shortcut")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: qsTr("Enable Plasma global shortcut")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Add configurable monitor controls to Plasma's Global Shortcuts settings.")
                        checked: controller.plasma_global_shortcuts
                        onToggled: controller.plasma_global_shortcuts = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    enabled: !controller.hide_tray_icon && settingsWindow.desktop.trayAvailable
                    Label {
                        text: qsTr("Hide the window when pressing on tray icon")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: qsTr("Hide the window when pressing on tray icon")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Click the tray icon to hide a visible window or show a hidden one.")
                        checked: controller.hide_on_tray_click
                        onToggled: controller.hide_on_tray_click = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    enabled: !controller.hide_tray_icon && settingsWindow.desktop.trayAvailable
                    Label {
                        text: qsTr("Close to tray icon")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        Accessible.name: qsTr("Close to tray icon")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Keep Brightless running after its window closes.")
                        checked: controller.close_to_tray
                        onToggled: controller.close_to_tray = checked
                    }
                }
            }
        }
    }

    Component {
        id: aboutPage
        ScrollView {
            id: aboutScroll
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: aboutScroll.availableWidth
                spacing: 12

                Image {
                    source: "qrc:/qt/qml/com/brightless/icon.png"
                    sourceSize.width: 96
                    sourceSize.height: 96
                    fillMode: Image.PreserveAspectFit
                    Accessible.ignored: true
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 96
                    Layout.alignment: Qt.AlignHCenter
                }
                Label {
                    text: "Brightless"
                    font.weight: Font.Medium
                    font.pointSize: settingsWindow.font.pointSize * 1.25
                    Layout.alignment: Qt.AlignHCenter
                }
                Label {
                    text: qsTr("Version %1").arg(Qt.application.version)
                    Layout.alignment: Qt.AlignHCenter
                }
                Button {
                    text: qsTr("Brightless repository")
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: Qt.openUrlExternally("https://github.com/sadesakaswl/brightless")
                }
                Label {
                    text: qsTr("GNU General Public License v3.0")
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
            }
        }
    }
}
