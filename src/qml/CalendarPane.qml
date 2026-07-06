import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Календарь на месяц: точки — дни с событиями, клик — события дня
Rectangle {
    id: pane
    radius: 12
    color: "white"
    implicitHeight: col.implicitHeight + 24

    property int year:  new Date().getFullYear()
    property int month: new Date().getMonth() + 1  // 1..12
    property string selectedDate: Qt.formatDate(new Date(), "yyyy-MM-dd")
    property var eventDays: []
    property var dayEvents: []
    property var cellsModel: []

    readonly property var monthNames: ["Январь", "Февраль", "Март", "Апрель",
                                       "Май", "Июнь", "Июль", "Август",
                                       "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"]

    function refresh() {
        eventDays = plannerManager.eventDaysInMonth(year, month)
        dayEvents = plannerManager.eventsForDate(selectedDate)

        const first = new Date(year, month - 1, 1)
        const offset = (first.getDay() + 6) % 7          // неделя с понедельника
        const daysInMonth = new Date(year, month, 0).getDate()
        let cells = []
        for (let i = 0; i < offset; i++)
            cells.push(0)
        for (let d = 1; d <= daysInMonth; d++)
            cells.push(d)
        cellsModel = cells
    }

    function shiftMonth(delta) {
        month += delta
        if (month < 1)  { month = 12; year-- }
        if (month > 12) { month = 1;  year++ }
        refresh()
    }

    function pad2(n) { return n < 10 ? "0" + n : "" + n }

    Component.onCompleted: refresh()

    Connections {
        target: plannerManager
        function onEventsChanged() { pane.refresh() }
    }

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Заголовок и навигация по месяцам
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text { text: "📅"; font.pixelSize: 16 }
            Text { text: "Календарь"; font.pixelSize: 16; font.bold: true }
            Item { Layout.fillWidth: true }

            Button {
                text: "‹"
                implicitWidth: 30; implicitHeight: 30
                onClicked: pane.shiftMonth(-1)
            }
            Text {
                text: monthNames[month - 1] + " " + year
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                Layout.preferredWidth: 110
            }
            Button {
                text: "›"
                implicitWidth: 30; implicitHeight: 30
                onClicked: pane.shiftMonth(1)
            }
            Button {
                text: "＋"
                implicitWidth: 30; implicitHeight: 30
                onClicked: addEventDialog.open()
            }
        }

        // Дни недели
        Grid {
            columns: 7
            Layout.fillWidth: true
            Repeater {
                model: ["Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"]
                Text {
                    width: (col.width) / 7
                    text: modelData
                    color: "#98A0AB"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // Сетка дней
        Grid {
            columns: 7
            Layout.fillWidth: true
            Repeater {
                model: pane.cellsModel
                Rectangle {
                    readonly property int day: modelData
                    readonly property string dateStr:
                        pane.year + "-" + pane.pad2(pane.month) + "-" + pane.pad2(day)
                    readonly property bool isToday:
                        dateStr === Qt.formatDate(new Date(), "yyyy-MM-dd")
                    readonly property bool isSelected: dateStr === pane.selectedDate

                    width: col.width / 7
                    height: 34
                    radius: 8
                    color: day === 0 ? "transparent"
                         : isSelected ? accentColor
                         : isToday    ? "#DCEBFF"
                                      : "transparent"

                    Text {
                        anchors.centerIn: parent
                        visible: day > 0
                        text: day
                        font.pixelSize: 13
                        color: isSelected ? "white" : "#1A1A1A"
                    }
                    Rectangle { // точка-маркер события
                        visible: day > 0 && pane.eventDays.indexOf(day) !== -1
                        width: 5; height: 5; radius: 2.5
                        color: isSelected ? "white" : "#FF9500"
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 3
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: day > 0
                        onClicked: {
                            pane.selectedDate = dateStr
                            pane.refresh()
                        }
                    }
                }
            }
        }

        // События выбранного дня
        Text {
            text: "События " + selectedDate.split("-").reverse().join(".")
            font.pixelSize: 12
            color: "#5B6470"
        }

        Repeater {
            model: pane.dayEvents
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: modelData.time !== "" ? modelData.time : "— : —"
                    font.pixelSize: 12
                    font.bold: true
                    color: accentColor
                    Layout.preferredWidth: 42
                }
                Column {
                    Layout.fillWidth: true
                    Text {
                        text: modelData.title
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        width: parent.width

                        // Клик по названию — редактирование
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: addEventDialog.openForEdit(modelData,
                                                                  pane.selectedDate)
                        }
                    }
                    Text {
                        visible: modelData.description !== ""
                        text: modelData.description
                        font.pixelSize: 11
                        color: "#8A929E"
                        wrapMode: Text.Wrap
                        width: parent.width
                    }
                }
                Button {
                    text: "✕"
                    implicitWidth: 26; implicitHeight: 26
                    onClicked: plannerManager.deleteEvent(modelData.id)
                }
            }
        }

        Text {
            visible: pane.dayEvents.length === 0
            text: "Нет событий"
            font.pixelSize: 12
            color: "#B0B7C0"
        }
    }

    AddEventDialog {
        id: addEventDialog
        presetDate: pane.selectedDate
    }
}
