import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    visible: true
    width: 480
    height: 800
    title: "🧠 Ассистент"

    property string serverUrl: "http://192.168.0.100:8080"
    property string history: ""
    property string currentModel: "Qwen2.5-7B.Q4_K_M"

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        // Шапка с переключателем
        Rectangle {
            Layout.fillWidth: true
            height: 100
            color: "#007AFF"
            radius: 0

            Column {
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: "🧠 Ассистент (Qwen)"
                    color: "white"
                    font.pixelSize: 20
                    font.bold: true
                }

                Row {
                    spacing: 8
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text {
                        text: "🚀 Быстро"
                        color: modelSwitch.checked ? "#B0D4F1" : "white"
                        font.bold: !modelSwitch.checked
                    }

                    Switch {
                        id: modelSwitch
                        checked: false

                        onCheckedChanged: {
                            if (checked) {
                                currentModel = "qwen2.5-14b-q4_k_m"
                                statusText.text = "🧠 Умный режим (14B)"
                            } else {
                                currentModel = "Qwen2.5-7B.Q4_K_M"
                                statusText.text = "🚀 Быстрый режим (7B)"
                            }
                        }
                    }

                    Text {
                        text: "🧠 Умно"
                        color: modelSwitch.checked ? "white" : "#B0D4F1"
                        font.bold: modelSwitch.checked
                    }
                }

                Text {
                    id: statusText
                    text: "🚀 Быстрый режим (7B)"
                    color: "white"
                    font.pixelSize: 12
                    opacity: 0.8
                }
            }
        }

        // Список сообщений
        ListView {
            id: messageListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
            clip: true
            model: messageModel
            delegate: messageDelegate
            ScrollBar.vertical: ScrollBar {}
        }

        // Индикатор загрузки
        Row {
            visible: loadingIndicator.visible
            spacing: 8
            Layout.leftMargin: 16
            Layout.bottomMargin: 4

            BusyIndicator {
                id: busyIndicator
                running: loadingIndicator.visible
                width: 24
                height: 24
            }

            Text {
                text: "Ассистент печатает..."
                color: "#666"
                font.italic: true
            }
        }

        // Поле ввода
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 8

            TextField {
                id: inputField
                Layout.fillWidth: true
                placeholderText: "Напишите сообщение..."
                onAccepted: sendMessage()
            }

            Button {
                text: "Отправить"
                onClicked: sendMessage()
            }
        }
    }

    ListModel { id: messageModel }

    Item {
        id: loadingIndicator
        visible: false
    }

    function sendMessage() {
        const text = inputField.text.trim()
        if (!text) return

        const now = new Date()
        const timeStr = now.toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit", second: "2-digit" })

        messageModel.append({ text: text, isUser: true, time: timeStr })
        inputField.clear()
        loadingIndicator.visible = true

        llamaClient.sendMessageWithHistory(text, serverUrl, history, currentModel)
    }

    Connections {
        target: llamaClient

        function onResponseReceived(response) {
            const now = new Date()
            const timeStr = now.toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit", second: "2-digit" })

            loadingIndicator.visible = false
            messageModel.append({ text: response, isUser: false, time: timeStr })
            messageListView.positionViewAtEnd()

            const lastUserMessage = messageModel.get(messageModel.count - 2).text
            history += "Пользователь: " + lastUserMessage + "\nАссистент: " + response + "\n"

            if (history.length > 1000) {
                history = history.substring(history.length - 1000)
            }
        }

        function onErrorOccurred(error) {
            loadingIndicator.visible = false
            messageModel.append({ text: "❌ Ошибка: " + error, isUser: false, time: "" })
            messageListView.positionViewAtEnd()
        }
    }

    Component {
        id: messageDelegate
        Rectangle {
            width: parent ? parent.width : 0
            height: messageColumn.height + 20
            color: isUser ? "#DCF8C6" : "#F0F0F0"
            radius: 16
            Layout.leftMargin: isUser ? parent.width / 3 : 8
            Layout.rightMargin: isUser ? 8 : parent.width / 3
            Layout.alignment: isUser ? Qt.AlignRight : Qt.AlignLeft

            Column {
                id: messageColumn
                anchors.centerIn: parent
                spacing: 2

                Text {
                    text: model.text
                    wrapMode: Text.Wrap
                    width: parent.width - 20
                    font.pixelSize: 16
                }

                Text {
                    text: model.time || ""
                    color: "#888"
                    font.pixelSize: 10
                    horizontalAlignment: isUser ? Text.AlignRight : Text.AlignLeft
                }
            }
        }
    }
}