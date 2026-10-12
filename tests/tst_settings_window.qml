import QtQuick
import QtTest

Item {
    id: root
    width: 640
    height: 640

    Component { id: backendComponent; FakeController {} }
    Window {
        id: host
        width: 640
        height: 640
        property var backend
        function refreshed(value) { backend.revision; return value }
    }
    QtObject {
        id: desktop
        property bool shortcutEditorAvailable: true
        property bool plasmaShortcutsAvailable: true
        property bool trayAvailable: true
        function configureShortcuts() {}
    }

    TestCase {
        name: "SettingsWindow"
        when: windowShown
        property var settings
        property var component
        property var backend

        function init() {
            failOnWarning(/.*/)
            backend = createTemporaryObject(backendComponent, root)
            host.backend = backend
            component = Qt.createComponent(Qt.resolvedUrl("../src/qt/qml/SettingsWindow.qml"))
            verify(component.status === Component.Ready, component.errorString())
            settings = component.createObject(null, { controller: backend, appWindow: host, desktop: desktop })
            verify(settings !== null)
        }

        function cleanup() {
            if (settings) settings.destroy()
            if (component) component.destroy()
        }

        function test_only_selected_page_is_loaded() {
            const loader = findChild(settings, "settingsPage")
            const category = findChild(settings, "settingsCategory")
            verify(!!loader && !!category, "Object exists")
            compare(loader.item, null)
            settings.open()
            tryCompare(loader, "status", Loader.Ready)
            compare(category.count, 6)
            backend.hide_volume = true
            backend.set_scroll_step(7)
            backend.add_timing()
            backend.use_12_hour_time = true
            for (const index of [1, 2, 3, 4, 0, 3]) {
                const previous = loader.item
                category.currentIndex = index
                tryCompare(loader, "status", Loader.Ready)
                verify(loader.item !== previous)
                if (index === 3) {
                    const format = findChild(loader.item, "timeFormatSelector")
                    verify(!!format, "Object exists")
                    compare(format.currentText, qsTr("12-hour (AM/PM)"))
                }
            }
            settings.hide()
            tryCompare(loader, "item", null)
            settings.open()
            tryCompare(loader, "status", Loader.Ready)
            compare(backend.hide_volume, true)
            compare(backend.scrollStep, 7)
            compare(backend.timings.length, 1)
        }

        function test_theme_selector() {
            const loader = findChild(settings, "settingsPage")
            settings.open()
            tryCompare(loader, "status", Loader.Ready)
            const selector = findChild(loader.item, "themeSelector")
            verify(!!selector)
            compare(selector.currentValue, "auto")
            for (const index of [1, 2, 0]) {
                while (selector.currentIndex < index) selector.incrementCurrentIndex()
                while (selector.currentIndex > index) selector.decrementCurrentIndex()
                selector.activated(index)
                compare(backend.theme_mode, ["auto", "light", "dark"][index])
            }
            backend.theme_mode = "dark"
            tryCompare(selector, "currentIndex", 2)
            settings.hide()
            tryCompare(loader, "item", null)
            settings.open()
            tryCompare(loader, "status", Loader.Ready)
            compare(findChild(loader.item, "themeSelector").currentValue, "dark")
        }

        function test_wheel_updates_slider_values_without_scrolling_page() {
            const loader = findChild(settings, "settingsPage")
            const category = findChild(settings, "settingsCategory")
            verify(!!loader && !!category, "Object exists")
            category.currentIndex = 1
            settings.open()
            tryCompare(loader, "status", Loader.Ready)
            const scrollStep = findChild(loader.item, "scrollStepSlider")
            const ddcDelay = findChild(loader.item, "ddcDelaySlider")
            const ratio = findChild(loader.item, "ratioSlider")
            const perMonitorRatio = findChild(loader.item, "perMonitorRatio")
            verify(!!scrollStep && !!ddcDelay && !!ratio && !!perMonitorRatio, "Object exists")
            const flickable = loader.item.contentItem
            const scrollPosition = flickable.contentY
            mouseWheel(scrollStep, scrollStep.width / 2, scrollStep.height / 2, 0, 120)
            tryCompare(backend, "scrollStep", 3)
            tryCompare(flickable, "contentY", scrollPosition)
            mouseWheel(ddcDelay, ddcDelay.width / 2, ddcDelay.height / 2, 0, 120)
            tryCompare(backend, "ddcDelay", 50)
            backend.set_dynamic_contrast_enabled(true)
            tryCompare(ratio, "visible", true)
            mouseWheel(ratio, ratio.width / 2, ratio.height / 2, 0, 120)
            tryVerify(() => Math.abs(backend.ratio - 0.8) < 0.000001)
            backend.set_dynamic_contrast_per_monitor_ratio(true)
            tryCompare(perMonitorRatio, "visible", true)
            mouseWheel(perMonitorRatio, perMonitorRatio.width / 2, perMonitorRatio.height / 2, 0, -120)
            tryVerify(() => Math.abs(backend.ratio - 0.7) < 0.000001)
            tryCompare(flickable, "contentY", scrollPosition)
            backend.set_scroll_step(10)
            mouseWheel(scrollStep, scrollStep.width / 2, scrollStep.height / 2, 0, 120)
            tryCompare(backend, "scrollStep", 10)
            tryCompare(flickable, "contentY", scrollPosition)
        }

        function test_small_window_and_large_font() {
            settings.width = 360
            settings.height = 320
            settings.font.pointSize = 18
            const loader = findChild(settings, "settingsPage")
            const category = findChild(settings, "settingsCategory")
            verify(!!loader && !!category, "Object exists")
            settings.open()
            backend.use_12_hour_time = true
            backend.add_timing()
            for (const index of [0, 1, 2, 3, 4]) {
                category.currentIndex = index
                tryCompare(loader, "status", Loader.Ready)
                if (index === 3) {
                    tryCompare(loader.item, "compact", true)
                    const format = findChild(loader.item, "timeFormatSelector")
                    verify(!!format, "Object exists")
                    compare(format.currentText, qsTr("12-hour (AM/PM)"))
                } else {
                    verify(loader.item.availableWidth > 0)
                    verify(loader.item.contentWidth <= loader.item.availableWidth)
                }
            }
            settings.hide()
            tryCompare(loader, "active", false)
        }
    }
}
