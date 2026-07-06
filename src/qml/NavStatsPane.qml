import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Статистика навигации: суммарные показатели и недавние маршруты
Rectangle {
    id: pane
    radius: 12
    color: "white"
    implicitHeight: col.implicitHeight + 24

    property var s: ({ count: 0, totalKm: "0", avgSpeed: "0", recent: [] })

    function refresh() {
        const st = navigationManager.stats()
        if (st && st.recent !== undefined) // до подключения кэш пуст
            s = st
    }
    Component.onCompleted: refresh()

    Connections {
        target: navigationManager
        function onNavStatsChanged() { pane.refresh() }
    }

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Text { text: "🗺️"; font.pixelSize: 16 }
            Text { text: "Навигация"; font.pixelSize: 16; font.bold: true }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    { label: "Маршрутов", value: pane.s.count },
                    { label: "Всего км",  value: pane.s.totalKm },
                    { label: "Ср. км/ч",  value: pane.s.avgSpeed }
                ]
                Rectangle {
                    Layout.fillWidth: true
                    height: 54
                    radius: 8
                    color: "#F4F7FA"

                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Text {
                            text: modelData.value
                            font.pixelSize: 17
                            font.bold: true
                            color: accentColor
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Text {
                            text: modelData.label
                            font.pixelSize: 10
                            color: "#8A929E"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }
            }
        }

        Text {
            visible: pane.s.recent.length > 0
            text: "Недавние маршруты"
            font.pixelSize: 12
            color: "#5B6470"
        }

        Repeater {
            model: pane.s.recent
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Text {
                    text: modelData.origin + " → " + modelData.destination
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Text {
                    text: modelData.date + " • " + modelData.distance + " • " + modelData.duration
                    font.pixelSize: 11
                    color: "#8A929E"
                }
            }
        }

        Text {
            visible: pane.s.recent.length === 0
            text: "Спроси в чате: «Как добраться до …» 🚶"
            font.pixelSize: 12
            color: "#B0B7C0"
        }
    }
}
