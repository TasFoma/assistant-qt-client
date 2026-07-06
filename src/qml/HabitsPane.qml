import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Прогресс и привычки: цели с индикаторами выполнения
Rectangle {
    id: pane
    radius: 12
    color: "white"
    implicitHeight: col.implicitHeight + 24

    property var items: []

    function refresh() { items = plannerManager.habits() }
    Component.onCompleted: refresh()

    Connections {
        target: plannerManager
        function onHabitsChanged() { pane.refresh() }
    }

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Text { text: "📈"; font.pixelSize: 16 }
            Text { text: "Прогресс и привычки"; font.pixelSize: 16; font.bold: true }
            Item { Layout.fillWidth: true }
            Button {
                text: "＋ Цель"
                font.pixelSize: 12
                implicitHeight: 30
                onClicked: addHabitDialog.open()
            }
        }

        Repeater {
            model: pane.items
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: modelData.name
                        font.pixelSize: 13
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Text {
                        text: modelData.progress + "/" + modelData.target + " "
                              + modelData.unit
                              + (modelData.period === "week" ? " в нед." : " в день")
                        font.pixelSize: 11
                        color: "#8A929E"
                    }
                    Button {
                        text: "＋"
                        implicitWidth: 26; implicitHeight: 26
                        onClicked: {
                            logHabitDialog.habitId = modelData.id
                            logHabitDialog.habitName = modelData.name
                            logHabitDialog.habitUnit = modelData.unit
                            logHabitDialog.open()
                        }
                    }
                    Button {
                        text: "✕"
                        implicitWidth: 26; implicitHeight: 26
                        onClicked: plannerManager.deleteHabit(modelData.id)
                    }
                }

                ProgressBar {
                    Layout.fillWidth: true
                    from: 0
                    to: modelData.target
                    value: Math.min(modelData.progress, modelData.target)
                }
            }
        }

        Text {
            visible: pane.items.length === 0
            text: "Добавь цель — например, «Английский, 30 мин в день»"
            font.pixelSize: 12
            color: "#B0B7C0"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    AddHabitDialog { id: addHabitDialog }

    // Быстрая запись прогресса по привычке
    Dialog {
        id: logHabitDialog
        property int habitId: -1
        property string habitName: ""
        property string habitUnit: ""

        title: "Отметить: " + habitName
        modal: true
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 280

        onOpened: valueField.text = "1"

        RowLayout {
            width: parent.width
            TextField {
                id: valueField
                Layout.fillWidth: true
                text: "1"
                validator: DoubleValidator { bottom: 0 }
            }
            Text { text: logHabitDialog.habitUnit; color: "#8A929E" }
        }

        onAccepted: {
            const v = parseFloat(valueField.text)
            if (!isNaN(v) && v > 0)
                plannerManager.logHabit(habitId, v)
        }
    }
}
