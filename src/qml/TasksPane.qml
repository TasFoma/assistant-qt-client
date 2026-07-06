import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Список задач: чекбоксы, приоритет, дедлайн
Rectangle {
    id: pane
    radius: 12
    color: "white"
    implicitHeight: col.implicitHeight + 24

    property var items: []

    function refresh() { items = plannerManager.tasks() }
    Component.onCompleted: refresh()

    Connections {
        target: plannerManager
        function onTasksChanged() { pane.refresh() }
    }

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 12
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Text { text: "✅"; font.pixelSize: 16 }
            Text { text: "Задачи"; font.pixelSize: 16; font.bold: true }
            Item { Layout.fillWidth: true }
            Button {
                text: "＋ Добавить"
                font.pixelSize: 12
                implicitHeight: 30
                onClicked: addTaskDialog.open()
            }
        }

        Repeater {
            model: pane.items
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                CheckBox {
                    checked: modelData.done
                    onToggled: plannerManager.setTaskDone(modelData.id, checked)
                }
                Rectangle { // индикатор приоритета
                    width: 4; height: 24; radius: 2
                    color: modelData.priority === 2 ? "#FF3B30"
                         : modelData.priority === 1 ? "#FF9500" : "#34C759"
                }
                Column {
                    Layout.fillWidth: true
                    Text {
                        text: modelData.title
                        font.pixelSize: 13
                        font.strikeout: modelData.done
                        color: modelData.done ? "#A0A7B0" : "#1A1A1A"
                        wrapMode: Text.Wrap
                        width: parent.width

                        // Клик по названию — редактирование
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: addTaskDialog.openForEdit(modelData)
                        }
                    }
                    Text {
                        visible: modelData.dueDate !== ""
                        text: "до " + modelData.dueDate
                        font.pixelSize: 11
                        color: "#8A929E"
                    }
                }
                Button {
                    text: "✕"
                    implicitWidth: 26; implicitHeight: 26
                    onClicked: plannerManager.deleteTask(modelData.id)
                }
            }
        }

        Text {
            visible: pane.items.length === 0
            text: "Задач нет — можно отдыхать 😌"
            font.pixelSize: 12
            color: "#B0B7C0"
        }
    }

    AddTaskDialog { id: addTaskDialog }
}
