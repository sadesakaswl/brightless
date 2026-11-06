import QtQuick
import QtTest
import "../src/qt/qml"

Item {
    id: root
    width: 500
    height: 700

    QtObject {
        id: desktop
        property bool shortcutsEnabled: false
        property string shortcutError: ""
        property var shortcutActions: []
        function keySequence(key, modifiers) { return "Ctrl+Alt+" + String.fromCharCode(key) }
        function setShortcut(id, sequence) {
            if (shortcutActions.some(action => action.id !== id && action.sequence === sequence && sequence)) {
                shortcutError = "Shortcut is already assigned"
                return false
            }
            shortcutActions = shortcutActions.map(action => action.id === id
                ? {id: id, text: action.text, sequence: sequence, displaySequence: sequence} : action)
            shortcutError = ""
            return true
        }
    }
    Component {
        id: shortcutsComponent
        GlobalShortcuts { width: 460; height: implicitHeight }
    }

    TestCase {
        name: "GlobalShortcuts"
        when: windowShown
        property var editor

        function init() {
            failOnWarning(/.*/)
            desktop.shortcutsEnabled = false
            desktop.shortcutError = ""
            desktop.shortcutActions = [
                {id: "brightness", text: "Brightness", sequence: "", displaySequence: ""},
                {id: "contrast", text: "Contrast", sequence: "", displaySequence: ""}
            ]
            editor = createTemporaryObject(shortcutsComponent, root, {desktop: desktop})
            verify(editor !== null)
        }

        function enable() {
            mouseClick(findChild(editor, "globalShortcutsSwitch"))
            tryCompare(desktop, "shortcutsEnabled", true)
        }

        function record(index, key) {
            const dropdown = findChild(editor, "newShortcutAction")
            dropdown.currentIndex = index
            mouseClick(findChild(editor, "recordShortcut"))
            const popup = findChild(editor, "shortcutRecorder")
            tryCompare(popup, "opened", true)
            keyClick(key, Qt.ControlModifier | Qt.AltModifier)
            return popup
        }

        function test_enable_and_disable() {
            compare(findChild(editor, "shortcutEditor"), null)
            enable()
            verify(findChild(editor, "newShortcutAction") !== null)
            mouseClick(findChild(editor, "globalShortcutsSwitch"))
            tryCompare(desktop, "shortcutsEnabled", false)
            tryVerify(() => findChild(editor, "shortcutEditor") === null)
        }

        function test_shortcuts_are_grouped_in_rounded_cards() {
            desktop.shortcutActions = [
                {id: "brightness", text: "Increase brightness and dynamic contrast", sequence: "Ctrl+Alt+B", displaySequence: "Ctrl+Alt+B"},
                {id: "contrast", text: "Contrast", sequence: "", displaySequence: ""}
            ]
            editor.width = 300
            enable()
            const assigned = findChild(editor, "shortcutCard_brightness")
            const pending = findChild(editor, "newShortcutCard")
            verify(!!assigned && !!pending, "Object exists")
            assigned.font.pointSize = 18
            pending.font.pointSize = 18
            findChild(pending, "newShortcutAction").currentIndex = 0
            waitForRendering(editor)
            for (const group of [
                {card: assigned, controls: ["shortcutBinding_brightness", "removeShortcut_brightness"]},
                {card: pending, controls: ["newShortcutAction", "recordShortcut"]}
            ]) {
                verify(group.card.background.radius > 0)
                compare(group.card.width, editor.width)
                for (const name of group.controls) {
                    const control = findChild(group.card, name)
                    verify(!!control, "Object exists")
                    verify(control.visible)
                    const position = control.mapToItem(group.card, 0, 0)
                    verify(position.x >= group.card.leftPadding - 1)
                    verify(position.y >= group.card.topPadding - 1)
                    verify(position.x + control.width <= group.card.width - group.card.rightPadding + 1)
                    verify(position.y + control.height <= group.card.height - group.card.bottomPadding + 1)
                }
            }
            compare(findChild(pending, "recordShortcut").visible, true)
        }

        function test_assign_another_and_remove() {
            enable()
            const popup = record(0, Qt.Key_B)
            tryCompare(popup, "visible", false)
            compare(desktop.shortcutActions[0].sequence, "Ctrl+Alt+B")
            const dropdown = findChild(editor, "newShortcutAction")
            compare(dropdown.model.length, 1)
            compare(dropdown.model[0].id, "contrast")
            compare(dropdown.displayText, qsTr("Add shortcut"))
            compare(findChild(editor, "recordShortcut").visible, false)
            record(0, Qt.Key_C)
            tryCompare(popup, "visible", false)
            compare(desktop.shortcutActions[1].sequence, "Ctrl+Alt+C")
            compare(dropdown.visible, false)
            compare(findChild(editor, "newShortcutCard").visible, false)
            waitForRendering(editor)
            mouseClick(findChild(editor, "removeShortcut_brightness"))
            tryCompare(dropdown, "visible", true)
            tryCompare(findChild(editor, "newShortcutCard"), "visible", true)
            compare(desktop.shortcutActions[0].sequence, "")
        }

        function test_conflict_keeps_recorder_open_and_escape_cancels() {
            desktop.shortcutActions = [
                {id: "brightness", text: "Brightness", sequence: "Ctrl+Alt+B", displaySequence: "Ctrl+Alt+B"},
                {id: "contrast", text: "Contrast", sequence: "", displaySequence: ""}
            ]
            enable()
            const popup = record(0, Qt.Key_B)
            tryCompare(desktop, "shortcutError", "Shortcut is already assigned")
            compare(popup.opened, true)
            compare(desktop.shortcutActions[1].sequence, "")
            keyClick(Qt.Key_Escape)
            tryCompare(popup, "visible", false)
            compare(desktop.shortcutActions[0].sequence, "Ctrl+Alt+B")
        }
    }
}
