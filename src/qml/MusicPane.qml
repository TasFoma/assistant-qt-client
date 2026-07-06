import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Музыка: локальная папка, перемешивание, пауза/след/пред
Rectangle {
    id: pane
    radius: 12
    color: "white"
    implicitHeight: col.implicitHeight + 24

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Text { text: "🎵"; font.pixelSize: 16 }
            Text { text: "Музыка"; font.pixelSize: 16; font.bold: true }
            Item { Layout.fillWidth: true }
            Button {
                text: "📁 Папка"
                font.pixelSize: 12
                implicitHeight: 30
                onClicked: folderDialog.open()
            }
        }

        // Название трека
        Text {
            Layout.fillWidth: true
            text: musicPlayer.trackCount === 0
                  ? "Выбери папку с музыкой — я перемешаю и включу 🎧"
                  : musicPlayer.trackTitle
            font.pixelSize: 13
            elide: Text.ElideRight
            color: musicPlayer.trackCount === 0 ? "#B0B7C0" : "#1A1A1A"
        }

        Text {
            visible: musicPlayer.trackCount > 0
            text: "Трек " + musicPlayer.trackIndex + " из " + musicPlayer.trackCount
            font.pixelSize: 11
            color: "#8A929E"
        }

        // Управление
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            Button {
                text: "⏮"
                font.pixelSize: 16
                implicitWidth: 44; implicitHeight: 40
                enabled: musicPlayer.trackCount > 0
                onClicked: musicPlayer.previous()
            }
            Button {
                text: musicPlayer.playing ? "⏸" : "▶"
                font.pixelSize: 18
                implicitWidth: 56; implicitHeight: 44
                enabled: musicPlayer.trackCount > 0
                background: Rectangle {
                    radius: 10
                    color: parent.pressed ? "#0056CC" : accentColor
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: musicPlayer.playPause()
            }
            Button {
                text: "⏭"
                font.pixelSize: 16
                implicitWidth: 44; implicitHeight: 40
                enabled: musicPlayer.trackCount > 0
                onClicked: musicPlayer.next()
            }
            Button {
                text: "🔀"
                font.pixelSize: 14
                implicitWidth: 40; implicitHeight: 40
                enabled: musicPlayer.trackCount > 0
                onClicked: musicPlayer.reshuffle()
                ToolTip.visible: hovered
                ToolTip.text: "Перемешать заново"
            }
        }

        // Громкость
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text { text: "🔈"; font.pixelSize: 12 }
            Slider {
                Layout.fillWidth: true
                from: 0; to: 1
                value: musicPlayer.volume
                onMoved: musicPlayer.volume = value
            }
            Text { text: "🔊"; font.pixelSize: 12 }
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Папка с музыкой"
        onAccepted: musicPlayer.setFolder(selectedFolder.toString())
    }
}
