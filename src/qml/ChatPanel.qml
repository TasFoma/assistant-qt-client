import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Левая колонка: чат с ассистентом. Вся логика — на сервере;
// здесь только отображение, голос и отправка текста.
Rectangle {
    id: chatPanel
    color: "white"

    function nowTime() {
        return Qt.formatTime(new Date(), "hh:mm")
    }

    function loadHistory() {
        messageModel.clear()
        const msgs = chatManager.loadMessages()
        for (let i = 0; i < msgs.length; i++)
            messageModel.append({ text: msgs[i].text,
                                  isUser: msgs[i].isUser,
                                  time: msgs[i].time })
        messageListView.positionViewAtEnd()
    }

    // Локальный пузырь (например, ошибка микрофона) — без записи на сервер
    function appendBubble(text, isUser) {
        messageModel.append({ text: text, isUser: isUser, time: nowTime() })
        messageListView.positionViewAtEnd()
    }

    function sendMessage() {
        const text = inputField.text.trim()
        if (text === "")
            return
        inputField.clear()
        typing.active = true
        // Сервер сам разберёт: команда → выполнит, вопрос → нейросеть,
        // маршрут → построит. Эхо и ответ придут сигналом messageAdded.
        backend.sendUserText(text, currentModel)
    }

    Component.onCompleted: loadHistory()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Шапка ────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 86
            color: accentColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Column {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "🧠 Мой спутник"
                        color: "white"
                        font.pixelSize: 20
                        font.bold: true
                    }
                    Text {
                        text: modelSwitch.checked ? "Умный режим (Qwen 14B)"
                                                  : "Быстрый режим (Qwen 7B)"
                        color: "white"
                        opacity: 0.85
                        font.pixelSize: 11
                    }
                    Text {
                        text: backend.connected && backend.authorized
                              ? "🔒 Сервер: " + backend.serverHost
                              : backend.connected
                                ? "🔑 Авторизация…"
                                : "🔓 Нет связи с сервером — переподключаюсь…"
                        color: "white"
                        opacity: 0.7
                        font.pixelSize: 10
                    }
                }

                Row {
                    spacing: 6
                    Text {
                        text: "🚀"
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: 14
                    }
                    Switch {
                        id: modelSwitch
                        checked: false // быстрая модель по умолчанию
                        onCheckedChanged:
                            currentModel = checked ? "qwen2.5-14b-instruct-q4_k_m"
                                                   : "Qwen2.5-7B-Instruct-Q4_K_M"
                    }
                    Text {
                        text: "🧠"
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: 14
                    }
                }

                Button {
                    text: "＋ Чат"
                    font.pixelSize: 12
                    implicitHeight: 32
                    onClicked: backend.newChat()
                    background: Rectangle {
                        radius: 8
                        color: parent.pressed ? "#0056CC" : "#3395FF"
                    }
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // Настройки сервера
                Button {
                    text: "⚙"
                    font.pixelSize: 14
                    implicitWidth: 32
                    implicitHeight: 32
                    onClicked: serverDialog.open()
                    background: Rectangle {
                        radius: 8
                        color: parent.pressed ? "#0056CC" : "#3395FF"
                    }
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // Показ/скрытие правой панели
                Button {
                    text: root.sidePanelOpen ? "▶" : "◀"
                    visible: root.width >= 1000
                    font.pixelSize: 12
                    implicitWidth: 32
                    implicitHeight: 32
                    onClicked: root.sidePanelOpen = !root.sidePanelOpen
                    background: Rectangle {
                        radius: 8
                        color: parent.pressed ? "#0056CC" : "#3395FF"
                    }
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // ── Лента сообщений ──────────────────────────────────────────
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

        // ── Индикатор «печатает» ─────────────────────────────────────
        Row {
            id: typing
            property bool active: false
            visible: active
            spacing: 8
            Layout.leftMargin: 16
            Layout.bottomMargin: 4

            BusyIndicator {
                running: typing.active
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

        // ── Панель ввода ─────────────────────────────────────────────
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

                // Скорость речи: 1x → 1.3x → 1.7x → 2x
                Button {
                    id: rateButton
                    readonly property var labels: ["1x", "1.3x", "1.7x", "2x"]
                    readonly property var rates:  [0,    2,      4,      6   ]
                    readonly property int  idx:   rates.indexOf(root.ttsRate)
                    text: labels[idx < 0 ? 0 : idx]
                    font.pixelSize: 11
                    implicitWidth: 38
                    implicitHeight: 44
                    background: Rectangle {
                        radius: 8
                        color: rateButton.pressed ? "#B0B0B0" : "#E8E8E8"
                    }
                    onClicked: root.ttsRate = rates[(idx + 1) % rates.length]
                }

                // TTS: ⏸ говорит (нажать = замолчать), 🔊 вкл, 🔇 выкл
                Button {
                    id: ttsButton
                    readonly property bool speaking: voiceInput && voiceInput.isSpeaking
                    text: speaking ? "⏸" : root.ttsEnabled ? "🔊" : "🔇"
                    font.pixelSize: 16
                    implicitWidth: 40
                    implicitHeight: 44
                    background: Rectangle {
                        radius: 8
                        color: ttsButton.speaking
                               ? (ttsButton.pressed ? "#C0392B" : "#E74C3C")
                               : root.ttsEnabled
                                 ? (ttsButton.pressed ? "#28A745" : "#34C759")
                                 : (ttsButton.pressed ? "#B0B0B0" : "#E8E8E8")
                    }
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
                            root.ttsEnabled = !root.ttsEnabled
                        }
                    }
                }

                // Микрофон: ⏳ модель грузится, 🎤 готов, ⏹ запись
                Button {
                    id: voiceButton
                    text: (voiceInput && voiceInput.isRecording) ? "⏹"
                        : (voiceInput && !voiceInput.isReady)    ? "⏳"
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
                    SequentialAnimation on opacity {
                        running: voiceInput && voiceInput.isRecording
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.35; duration: 550; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0;  duration: 550; easing.type: Easing.InOutSine }
                        onStopped: voiceButton.opacity = 1.0
                    }
                    RotationAnimation on rotation {
                        running: voiceInput && !voiceInput.isReady && !voiceInput.isRecording
                        loops: Animation.Infinite
                        from: 0; to: 360; duration: 1500
                        onStopped: voiceButton.rotation = 0
                    }
                    onClicked: {
                        if (voiceInput && voiceInput.isRecording)
                            voiceInput.stopRecording()
                        else if (voiceInput)
                            voiceInput.startRecording()
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

    // ── Настройки подключения к серверу ──────────────────────────────
    Dialog {
        id: serverDialog
        title: "⚙ Сервер ассистента"
        modal: true
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 400

        onOpened: {
            hostField.text = backend.serverHost
            tokenField.text = backend.token()
        }

        ColumnLayout {
            width: parent.width
            spacing: 8

            TextField {
                id: hostField
                Layout.fillWidth: true
                placeholderText: "IP сервера (например 192.168.0.100)"
            }
            TextField {
                id: tokenField
                Layout.fillWidth: true
                placeholderText: "Токен синхронизации"
                font.pixelSize: 11
            }
        }

        onAccepted: {
            backend.setServerHost(hostField.text)
            backend.setToken(tokenField.text)
            backend.reconnect()
        }
    }

    // Всплывающая подсказка «Скопировано» (правый клик по сообщению)
    Rectangle {
        id: copiedHint
        visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 80
        width: copiedText.implicitWidth + 24
        height: 34
        radius: 17
        color: "#CC000000"
        z: 100

        Text {
            id: copiedText
            anchors.centerIn: parent
            text: "📋 Скопировано"
            color: "white"
            font.pixelSize: 13
        }

        Timer {
            id: copiedHintTimer
            interval: 1500
            onTriggered: copiedHint.visible = false
        }
    }

    // ── Сигналы сервера ──────────────────────────────────────────────
    Connections {
        target: backend

        function onMessageAdded(text, isUser) {
            appendBubble(text, isUser)
            if (!isUser) {
                typing.active = false
                if (root.ttsEnabled && voiceInput)
                    voiceInput.speak(text, root.ttsRate)
            }
        }

        function onHistoryLoaded() {
            loadHistory()
        }

        function onChatError(error) {
            typing.active = false
            appendBubble("❌ " + error, false)
        }
    }

    // ── Сигналы голосового ввода ─────────────────────────────────────
    Connections {
        target: voiceInput

        function onTextRecognized(text) {
            inputField.text = text
            sendMessage()
        }

        function onErrorOccurred(error) {
            console.warn("Voice error:", error)
            appendBubble("🎤 " + error, false)
        }
    }

    // ── Делегат сообщения ────────────────────────────────────────────
    Component {
        id: messageDelegate
        Item {
            width: ListView.view ? ListView.view.width : 0
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
                    color: isUser ? accentColor : "#999"

                    Text {
                        anchors.centerIn: parent
                        text: isUser ? "👤" : "🧠"
                        font.pixelSize: 18
                    }
                }

                Rectangle {
                    width: Math.min(messageText.implicitWidth + 24,
                                    chatPanel.width * 0.68)
                    height: Math.max(40, messageColumn.implicitHeight + 16)
                    color: isUser ? userBubble : assistantBubble
                    radius: 12

                    Column {
                        id: messageColumn
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 2

                        // TextEdit: текст можно выделять мышью и копировать
                        // (Ctrl+C); правый клик — скопировать всё
                        TextEdit {
                            id: messageText
                            text: model.text
                            wrapMode: Text.Wrap
                            width: parent.width
                            font.pixelSize: 15
                            color: "#1A1A1A"
                            readOnly: true
                            selectByMouse: true
                            selectionColor: accentColor
                            textFormat: isUser ? TextEdit.PlainText : TextEdit.MarkdownText
                            onLinkActivated: (link) => Qt.openUrlExternally(link)

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                cursorShape: Qt.IBeamCursor
                                onClicked: {
                                    messageText.selectAll()
                                    messageText.copy()
                                    messageText.deselect()
                                    copiedHint.visible = true
                                    copiedHintTimer.restart()
                                }
                            }
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
