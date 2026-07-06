import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    visibility: Window.Maximized
    minimumWidth: 720
    minimumHeight: 560
    title: "🧠 Мой спутник"

    // ── Глобальные настройки ─────────────────────────────────────────
    property string currentModel: "Qwen2.5-7B-Instruct-Q4_K_M"
    property bool   ttsEnabled: true
    // SAPI rate 0/2/4/6 → ≈ 1x / 1.3x / 1.7x / 2x
    property int    ttsRate: 2

    readonly property color accentColor:     "#007AFF"
    readonly property color userBubble:      "#DCF8C6"
    readonly property color assistantBubble: "#F0F0F0"

    // На узких экранах правая панель сворачивается
    property bool sidePanelOpen: true
    readonly property bool sidePanelVisible: sidePanelOpen && width >= 1000

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ChatPanel {
            id: chatPanel
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredWidth: root.width * 0.55
        }

        Rectangle { // разделитель
            visible: root.sidePanelVisible
            Layout.fillHeight: true
            width: 1
            color: "#D8DCE2"
        }

        SidePanel {
            visible: root.sidePanelVisible
            Layout.fillHeight: true
            Layout.preferredWidth: root.width * 0.45
        }
    }

    // Напоминания озвучиваем (пузырь в чате появится через messageAdded)
    Connections {
        target: backend

        function onReminderDue(title) {
            if (root.ttsEnabled && voiceInput)
                voiceInput.speak("Напоминание: " + title, root.ttsRate)
        }
    }
}
