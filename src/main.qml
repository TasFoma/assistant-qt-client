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
    property string currentModel: "qwen2.5-14b-q4_k_m"
    property bool   ttsEnabled: true
    // SAPI rate 0/2/4/6 → ≈ 1x / 1.3x / 1.7x / 2x
    property int    ttsRate: 2

    readonly property color userColor:       "#007AFF"
    readonly property color userBubble:      "#DCF8C6"
    readonly property color assistantBubble: "#F0F0F0"
    readonly property color accentColor:     "#007AFF"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Шапка ────────────────────────────────────────────────────────────
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
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Row {
                    spacing: 8
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text {
                        text: "🚀 Быстро"
                        color: modelSwitch.checked ? "#B0D4F1" : "white"
                        font.bold: !modelSwitch.checked
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Switch {
                        id: modelSwitch
                        checked: true          // 14B по умолчанию
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
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    id: statusText
                    text: "🧠 Умный режим (14B)"
                    color: "white"
                    font.pixelSize: 11
                    opacity: 0.8
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // ── Список сообщений ─────────────────────────────────────────────────
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

        // ── Индикатор «печатает» ─────────────────────────────────────────────
        Row {
            visible: loadingIndicator.visible
            spacing: 8
            Layout.leftMargin: 16
            Layout.bottomMargin: 4

            BusyIndicator {
                running: loadingIndicator.visible
                width: 20
                height: 20
            }

            Text {
                text: "Ассистент думает..."
                color: "#666"
                font.italic: true
                font.pixelSize: 13
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // ── Панель ввода ─────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 60
            color: "white"
            border.color: "#E0E0E0"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                TextField {
                    id: inputField
                    Layout.fillWidth: true
                    placeholderText: voiceInput && voiceInput.isRecording
                                     ? "🎤 Слушаю... говорите"
                                     : "Напишите или скажите сообщение..."
                    font.pixelSize: 15
                    onAccepted: sendMessage()
                }

                // ── Скорость речи ─────────────────────────────────────────
                Button {
                    id: rateButton
                    // cycle: 1x → 1.3x → 1.7x → 2x → 1x
                    readonly property var labels: ["1x", "1.3x", "1.7x", "2x"]
                    readonly property var rates:  [0,    2,      4,      6   ]
                    readonly property int  idx:   rates.indexOf(ttsRate)
                    text: labels[idx < 0 ? 0 : idx]
                    font.pixelSize: 11
                    implicitWidth: 38
                    implicitHeight: 44
                    background: Rectangle {
                        radius: 8
                        color: rateButton.pressed ? "#B0B0B0" : "#E8E8E8"
                    }
                    onClicked: {
                        const next = (idx + 1) % rates.length
                        ttsRate = rates[next]
                    }
                }

                // ── TTS-кнопка ────────────────────────────────────────────
                // ⏸ пока говорит (нажать = замолчать)
                // 🔊 включена, молчит  (нажать = выкл)
                // 🔇 выключена         (нажать = вкл)
                Button {
                    id: ttsButton
                    readonly property bool speaking: voiceInput && voiceInput.isSpeaking
                    text: speaking ? "⏸" : ttsEnabled ? "🔊" : "🔇"
                    font.pixelSize: 16
                    implicitWidth: 40
                    implicitHeight: 44
                    background: Rectangle {
                        radius: 8
                        color: ttsButton.speaking
                               ? (ttsButton.pressed ? "#C0392B" : "#E74C3C")   // красный — стоп
                               : ttsEnabled
                                 ? (ttsButton.pressed ? "#28A745" : "#34C759") // зелёный — вкл
                                 : (ttsButton.pressed ? "#B0B0B0" : "#E8E8E8") // серый  — выкл
                    }
                    // Пульсация пока говорит
                    SequentialAnimation on opacity {
                        running: ttsButton.speaking
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.5; duration: 600; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 600; easing.type: Easing.InOutSine }
                        onStopped: ttsButton.opacity = 1.0
                    }
                    onClicked: {
                        if (speaking) {
                            if (voiceInput) voiceInput.stopSpeaking()
                        } else {
                            ttsEnabled = !ttsEnabled
                        }
                    }
                }

                // ── Кнопка микрофона ──────────────────────────────────────
                Button {
                    id: voiceButton
                    // ⏳ пока демон грузит модель, 🎤 готов, ⏹ запись идёт
                    text: (voiceInput && voiceInput.isRecording)          ? "⏹"
                        : (voiceInput && !voiceInput.isReady)             ? "⏳"
                                                                          : "🎤"
                    font.pixelSize: 18
                    implicitWidth: 44
                    implicitHeight: 44
                    enabled: !voiceInput || voiceInput.isReady || voiceInput.isRecording

                    background: Rectangle {
                        radius: 8
                        color: voiceInput && voiceInput.isRecording ? "#FF3B30"
                             : voiceButton.enabled && voiceButton.pressed ? "#C0C0C0"
                             : voiceButton.enabled                        ? "#E8E8E8"
                                                                          : "#D0D0D0"
                    }

                    // Пульсация во время записи
                    SequentialAnimation on opacity {
                        running: voiceInput && voiceInput.isRecording
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.35; duration: 550; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0;  duration: 550; easing.type: Easing.InOutSine }
                        onStopped: voiceButton.opacity = 1.0
                    }

                    // Вращение пока модель грузится
                    RotationAnimation on rotation {
                        running: voiceInput && !voiceInput.isReady && !voiceInput.isRecording
                        loops: Animation.Infinite
                        from: 0; to: 360; duration: 1500
                        onStopped: voiceButton.rotation = 0
                    }

                    onClicked: {
                        if (voiceInput && voiceInput.isRecording) {
                            voiceInput.stopRecording()
                        } else if (voiceInput) {
                            voiceInput.startRecording()
                        }
                    }
                }

                Button {
                    text: "→"
                    font.pixelSize: 18
                    implicitWidth: 40
                    implicitHeight: 44
                    background: Rectangle {
                        radius: 8
                        color: parent.pressed ? "#0056CC" : accentColor
                    }
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
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

    // ── Сигналы голосового ввода ─────────────────────────────────────────────
    Connections {
        target: voiceInput

        function onTextRecognized(text) {
            inputField.text = text
            sendMessage()
        }

        function onErrorOccurred(error) {
            console.warn("Voice error:", error)
            const now = new Date()
            const timeStr = now.toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit" })
            messageModel.append({ text: "🎤 " + error, isUser: false, time: timeStr })
            messageListView.positionViewAtEnd()
        }
    }

    // ── Сигналы ответа ассистента ────────────────────────────────────────────
    Connections {
        target: llamaClient

        function onResponseReceived(response) {
            const now = new Date()
            const timeStr = now.toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit" })

            loadingIndicator.visible = false
            messageModel.append({ text: response, isUser: false, time: timeStr })
            messageListView.positionViewAtEnd()

            const lastUserMessage = messageModel.get(messageModel.count - 2).text
            // ChatML format so the model sees proper turn structure
            history += "<|im_start|>user\n" + lastUserMessage + "\n<|im_end|>\n"
                     + "<|im_start|>assistant\n" + response + "\n<|im_end|>\n"
            if (history.length > 4000) {
                history = history.substring(history.length - 4000)
            }

            if (ttsEnabled && voiceInput) voiceInput.speak(response, ttsRate)
        }

        function onErrorOccurred(error) {
            loadingIndicator.visible = false
            const now = new Date()
            const timeStr = now.toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit" })
            messageModel.append({ text: "❌ " + error, isUser: false, time: timeStr })
            messageListView.positionViewAtEnd()
        }
    }

    // ── Делегат сообщения ────────────────────────────────────────────────────
    Component {
        id: messageDelegate
        Item {
            width: parent ? parent.width : 0
            height: Math.max(50, row.implicitHeight + 16)

            Row {
                id: row
                anchors {
                    left:    isUser ? undefined : parent.left
                    right:   isUser ? parent.right : undefined
                    margins: 8
                }
                spacing: 8

                Rectangle {
                    width: 36; height: 36; radius: 18
                    color: isUser ? userColor : "#999"

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
