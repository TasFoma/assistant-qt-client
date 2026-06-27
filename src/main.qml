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

    readonly property color userColor: "#007AFF"
    readonly property color userBubble: "#DCF8C6"
    readonly property color assistantBubble: "#F0F0F0"
    readonly property color accentColor: "#007AFF"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            height: 90
            color: accentColor

            Column {
                anchors.centerIn: parent
                spacing: 4

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
                    font.pixelSize: 11
                    opacity: 0.8
                }
            }
        }

        ListView {
            id: messageListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            clip: true
            model: messageModel
            delegate: messageDelegate
            ScrollBar.vertical: ScrollBar {}
        }

        Row {
            visible: loadingIndicator.visible
            spacing: 8
            Layout.leftMargin: 16
            Layout.bottomMargin: 4

            BusyIndicator {
                id: busyIndicator
                running: loadingIndicator.visible
                width: 20
                height: 20
            }

            Text {
                text: "Ассистент печатает..."
                color: "#666"
                font.italic: true
                font.pixelSize: 13
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 60
            color: "white"
            border.color: "#E0E0E0"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                TextField {
                    id: inputField
                    Layout.fillWidth: true
                    placeholderText: "Напишите или скажите сообщение..."
                    font.pixelSize: 15
                    onAccepted: sendMessage()
                }

                Button {
                    id: voiceButton
                    text: voiceInput.isRecording ? "⏹"
                        : voiceInput.isReady     ? "🎤"
                                                 : "⏳"
                    font.pixelSize: 18
                    implicitWidth: 44
                    implicitHeight: 44
                    enabled: voiceInput.isReady || voiceInput.isRecording

                    background: Rectangle {
                        radius: 8
                        color: voiceInput.isRecording ? "#FF3B30"
                             : voiceInput.isReady     ? (voiceButton.pressed ? "#C0C0C0" : "#E8E8E8")
                                                      : "#D0D0D0"
                    }

                    SequentialAnimation on opacity {
                        running: voiceInput.isRecording
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.35; duration: 550; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0;  duration: 550; easing.type: Easing.InOutSine }
                        onStopped: voiceButton.opacity = 1.0
                    }

                    // Spinner while model is loading
                    RotationAnimation on rotation {
                        running: !voiceInput.isReady && !voiceInput.isRecording
                        loops: Animation.Infinite
                        from: 0; to: 360; duration: 1200
                        onStopped: voiceButton.rotation = 0
                    }

                    onClicked: {
                        if (voiceInput.isRecording) {
                            voiceInput.stopRecording()
                        } else {
                            voiceInput.startRecording()
                        }
                    }
                }

                Button {
                    text: "Отправить"
                    font.pixelSize: 14
                    onClicked: sendMessage()
                }
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
        const timeStr = now.toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit" })

        messageModel.append({ text: text, isUser: true, time: timeStr })
        inputField.clear()
        loadingIndicator.visible = true

        llamaClient.searchAndAnswer(text, serverUrl, history, currentModel)
    }

    Connections {
        target: voiceInput

        function onTextRecognized(text) {
            inputField.text = text
            sendMessage()
        }

        function onErrorOccurred(error) {
            console.warn("Voice error:", error)
            inputField.placeholderText = "Ошибка микрофона"
        }
    }

    Connections {
        target: llamaClient

        function onResponseReceived(response) {
            const now = new Date()
            const timeStr = now.toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit" })

            loadingIndicator.visible = false
            messageModel.append({ text: response, isUser: false, time: timeStr })
            messageListView.positionViewAtEnd()

            const lastUserMessage = messageModel.get(messageModel.count - 2).text
            history += "Пользователь: " + lastUserMessage + "\nАссистент: " + response + "\n"

            if (history.length > 1000) {
                history = history.substring(history.length - 1000)
            }

            // Озвучивание — пока отключено, чтобы не мешало
            // voiceInput.speak(response)
        }

        function onErrorOccurred(error) {
            loadingIndicator.visible = false
            messageModel.append({ text: "❌ Ошибка: " + error, isUser: false, time: "" })
            messageListView.positionViewAtEnd()
        }
    }

    Component {
        id: messageDelegate
        Item {
            width: parent ? parent.width : 0
            height: Math.max(50, row.implicitHeight + 16)

            Row {
                id: row
                anchors {
                    left: isUser ? undefined : parent.left
                    right: isUser ? parent.right : undefined
                    margins: 8
                }
                spacing: 8

                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    color: isUser ? userColor : "#999"
                    visible: true

                    Text {
                        anchors.centerIn: parent
                        text: isUser ? "👤" : "🧠"
                        font.pixelSize: 18
                    }
                }

                Rectangle {
                    width: Math.min(messageText.implicitWidth + 24, parent.parent.width * 0.65)
                    height: Math.max(40, messageColumn.implicitHeight + 16)
                    color: isUser ? userBubble : assistantBubble
                    radius: 12

                    Column {
                        id: messageColumn
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 2

                        Text {
                            id: messageText
                            text: model.text
                            wrapMode: Text.Wrap
                            width: parent.width
                            font.pixelSize: 15
                            color: "#1A1A1A"
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
    }
}