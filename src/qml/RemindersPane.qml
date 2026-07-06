import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Напоминания: по времени (⏰) и по геолокации (📍)
Rectangle {
    id: pane
    radius: 12
    color: "white"
    implicitHeight: col.implicitHeight + 24

    property var items: []

    function refresh() { items = plannerManager.reminders() }
    Component.onCompleted: refresh()

    Connections {
        target: plannerManager
        function onRemindersChanged() { pane.refresh() }
    }

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 12
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Text { text: "⏰"; font.pixelSize: 16 }
            Text { text: "Напоминания"; font.pixelSize: 16; font.bold: true }
            Item { Layout.fillWidth: true }
            Button {
                text: "＋ Добавить"
                font.pixelSize: 12
                implicitHeight: 30
                onClicked: addReminderDialog.open()
            }
        }

        Repeater {
            model: pane.items
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: modelData.type === "geo" ? "📍" : "⏰"
                    font.pixelSize: 16
                }
                Column {
                    Layout.fillWidth: true
                    Text {
                        text: modelData.title
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        width: parent.width
                    }
                    Text {
                        text: modelData.type === "geo"
                              ? "около: " + modelData.place
                              : modelData.dueAt
                        font.pixelSize: 11
                        color: "#8A929E"
                    }
                }
                Button {
                    text: "✕"
                    implicitWidth: 26; implicitHeight: 26
                    onClicked: plannerManager.deleteReminder(modelData.id)
                }
            }
        }

        Text {
            visible: pane.items.length === 0
            text: "Активных напоминаний нет"
            font.pixelSize: 12
            color: "#B0B7C0"
        }
    }

    AddReminderDialog { id: addReminderDialog }
}
