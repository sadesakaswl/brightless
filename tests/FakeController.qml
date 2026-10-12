import QtQml

QtObject {
    property int revision: 0
    property int monitor_count: 1
    property var monitor_names: ["Test display"]
    property var ddc_monitors: [{ id: "display-0", name: "Test display" }]
    property bool loading: false
    property string theme_mode: "auto"
    property bool hide_brightness: false
    property bool hide_contrast: false
    property bool hide_volume: false
    property bool hide_input: false
    property bool hide_tray_icon: false
    property bool autostart: false
    property bool autostart_as_tray_icon: false
    property bool plasma_global_shortcuts: false
    property bool hide_on_tray_click: true
    property bool close_to_tray: true
    property bool use_12_hour_time: false
    property var timings: []
    property int battery_percent: -1
    property bool hasContrast: true
    property bool hasVolume: true
    property bool hasInput: true
    property bool hasPower: true
    property bool dynamic: false
    property bool globalDynamic: true
    property bool perMonitorDynamic: false
    property bool perMonitorRatio: false
    property real ratio: 0.7
    property bool perMonitorVcp: false
    property int brightnessValue: 50
    property int contrastValue: 40
    property int volumeValue: 30
    property int inputCode: 17
    property int powerCode: 1
    property int scrollStep: 2
    property int ddcDelay: 0
    property int scans: 0
    property string lastWrite: ""
    property var vcpCodes: ({})

    function initialize() { scans++ }
    function supports_contrast(index) { return hasContrast }
    function supports_volume(index) { return hasVolume }
    function supports_input_source(index) { return hasInput }
    function supports_power_mode(index) { return hasPower }
    function brightness(index) { return brightnessValue }
    function contrast(index) { return contrastValue }
    function volume(index) { return volumeValue }
    function input_source_code(index) { return inputCode }
    function power_mode_code(index) { return powerCode }
    function set_brightness(index, value) { brightnessValue = value; lastWrite = "brightness"; revision++ }
    function set_contrast(index, value) { contrastValue = value; lastWrite = "contrast"; revision++ }
    function set_volume(index, value) { volumeValue = value; lastWrite = "volume"; revision++ }
    function set_input_source(index, code) { inputCode = code; revision++ }
    function set_power_mode(index, code) { powerCode = code; revision++ }
    function scroll_step() { return scrollStep }
    function set_scroll_step(value) { scrollStep = value; revision++ }
    function ddc_delay() { return ddcDelay }
    function set_ddc_delay(value) { ddcDelay = value; revision++ }
    function dynamic_contrast_enabled() { return dynamic }
    function set_dynamic_contrast_enabled(value) { dynamic = value; revision++ }
    function dynamic_contrast_global() { return globalDynamic }
    function set_dynamic_contrast_global(value) { globalDynamic = value; revision++ }
    function dynamic_contrast_ratio() { return ratio }
    function set_dynamic_contrast_ratio(value) { ratio = value; revision++ }
    function dynamic_contrast_per_monitor_ratio() { return perMonitorRatio }
    function set_dynamic_contrast_per_monitor_ratio(value) { perMonitorRatio = value; revision++ }
    function monitor_dynamic_contrast_enabled(index) { return hasContrast && dynamic && (globalDynamic || perMonitorDynamic) }
    function set_monitor_dynamic_contrast_enabled(index, value) { perMonitorDynamic = value; revision++ }
    function monitor_ratio(index) { return ratio }
    function set_monitor_ratio(index, value) { ratio = value; revision++ }
    function set_dynamic_contrast_brightness(index, value) { brightnessValue = value; lastWrite = "dynamic"; revision++ }
    function vcp_per_monitor() { return perMonitorVcp }
    function set_vcp_per_monitor(value) { perMonitorVcp = value; revision++ }
    function vcp_code(code, scope) { return vcpCodes[code] ?? code }
    function set_vcp_code(code, value, scope) { vcpCodes[code] = value; revision++; return true }
    function add_timing() {
        timings = timings.concat([{ trigger: "time", time: "12:00", battery_percent: 20, battery_condition: "below",
                                    monitor_id: "", control: 0x10, value: 50, enabled: false }])
    }
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
}
