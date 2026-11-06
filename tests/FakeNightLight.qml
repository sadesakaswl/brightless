import QtQml

QtObject {
    property bool available: true
    property bool busy: false
    property bool sharedSchedule: false
    property string error: ""
    property var settings: ({ enabled: false, mode: 1, temperature: 4500, dayTemperature: 6500,
                              evening: "18:00", morning: "06:00", transition: 30,
                              automaticLocation: true, latitude: 0, longitude: 0 })
    property var lastApplied: null
    property int locationRequests: 0
    signal locationDetected(real latitude, real longitude)
    function initialize() {}
    function detectLocation() { locationRequests++ }
    function apply(value) { lastApplied = value; settings = value }
}
