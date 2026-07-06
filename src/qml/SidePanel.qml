import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Правая колонка: календарь, задачи, напоминания, привычки, навигация
Rectangle {
    id: sidePanel
    color: "#EEF1F5"

    Flickable {
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 12

            MusicPane     { Layout.fillWidth: true }
            CalendarPane  { Layout.fillWidth: true }
            TasksPane     { Layout.fillWidth: true }
            RemindersPane { Layout.fillWidth: true }
            HabitsPane    { Layout.fillWidth: true }
            NavStatsPane  { Layout.fillWidth: true }

            Item { height: 8 } // нижний отступ
        }
    }
}
