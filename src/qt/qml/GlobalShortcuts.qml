pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: shortcuts
    required property var desktop
    spacing: 12

    RowLayout {
        Layout.fillWidth: true
        Label {
            text: qsTr("Enable global shortcuts")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Switch {
            objectName: "globalShortcutsSwitch"
            Accessible.name: qsTr("Enable global shortcuts")
            checked: shortcuts.desktop.shortcutsEnabled
            onToggled: shortcuts.desktop.shortcutsEnabled = checked
        }
    }

    Label {
        objectName: "shortcutError"
        Layout.fillWidth: true
        visible: text.length > 0
        text: shortcuts.desktop.shortcutError
        wrapMode: Text.WordWrap
    }

    Loader {
        Layout.fillWidth: true
        active: shortcuts.desktop.shortcutsEnabled
        sourceComponent: ColumnLayout {
            id: editor
            objectName: "shortcutEditor"
            spacing: 8
            readonly property var availableActions: shortcuts.desktop.shortcutActions.filter(action => !action.sequence)

            Repeater {
                model: shortcuts.desktop.shortcutActions.filter(action => action.sequence)
                delegate: ControlCard {
                    id: assignment
                    required property var modelData
                    objectName: "shortcutCard_" + modelData.id
                    Layout.fillWidth: true
                    padding: 12

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8
                        Label {
                            text: assignment.modelData.text
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Button {
                                objectName: "shortcutBinding_" + assignment.modelData.id
                                text: assignment.modelData.displaySequence
                                Accessible.name: assignment.modelData.text + ": " + text
                                Layout.fillWidth: true
                                onClicked: recorder.record(assignment.modelData.id)
                            }
                            Button {
                                objectName: "removeShortcut_" + assignment.modelData.id
                                text: qsTr("Remove")
                                Accessible.name: text + ": " + assignment.modelData.text
                                onClicked: shortcuts.desktop.setShortcut(assignment.modelData.id, "")
                            }
                        }
                    }
                }
            }

            ControlCard {
                objectName: "newShortcutCard"
                Layout.fillWidth: true
                visible: newAction.count > 0
                padding: 12

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    ComboBox {
                        id: newAction
                        objectName: "newShortcutAction"
                        Layout.fillWidth: true
                        Accessible.name: qsTr("Add shortcut")
                        displayText: currentIndex < 0 ? qsTr("Add shortcut") : currentText
                        model: editor.availableActions
                        textRole: "text"
                        valueRole: "id"
                        currentIndex: -1
                    }
                    Button {
                        objectName: "recordShortcut"
                        Layout.fillWidth: true
                        visible: newAction.currentIndex >= 0
                        text: qsTr("Set shortcut")
                        onClicked: recorder.record(newAction.currentValue)
                    }
                }
            }

            Popup {
                id: recorder
                objectName: "shortcutRecorder"
                property string actionId
                parent: Overlay.overlay
                anchors.centerIn: parent
                width: Math.min(360, parent.width - 32)
                modal: true
                focus: true

                function record(id: string): void {
                    actionId = id;
                    open();
                }

                contentItem: ColumnLayout {
                    focus: true
                    spacing: 12
                    Keys.onShortcutOverride: event => event.accepted = true
                    Keys.onPressed: event => {
                        event.accepted = true;
                        if (event.key === Qt.Key_Escape) {
                            recorder.close();
                            return;
                        }
                        if (event.isAutoRepeat || [Qt.Key_Shift, Qt.Key_Control, Qt.Key_Alt,
                                                  Qt.Key_Meta, Qt.Key_AltGr, Qt.Key_unknown].includes(event.key))
                            return;
                        const sequence = shortcuts.desktop.keySequence(event.key, event.modifiers);
                        if (shortcuts.desktop.setShortcut(recorder.actionId, sequence)) {
                            newAction.currentIndex = -1;
                            recorder.close();
                        }
                    }
                    Label {
                        text: qsTr("Press a key combination. Escape cancels.")
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        text: shortcuts.desktop.shortcutError
                        visible: text.length > 0
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Button {
                        text: qsTr("Cancel")
                        onClicked: recorder.close()
                    }
                }
            }
        }
    }
}
