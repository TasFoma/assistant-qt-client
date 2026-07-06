import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dlg
    title: "📈 Новая цель"
    modal: true
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 380

    onOpened: {
        nameField.text = ""
        targetField.text = ""
        unitField.text = "мин"
        periodBox.currentIndex = 0
        nameField.forceActiveFocus()
    }

    ColumnLayout {
        width: parent.width
        spacing: 8

        TextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: "Название (например, «Английский»)"
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: targetField
                Layout.fillWidth: true
                placeholderText: "Цель (число)"
                validator: DoubleValidator { bottom: 0 }
            }
            TextField {
                id: unitField
                Layout.preferredWidth: 90
                placeholderText: "мин / раз / стр"
            }
        }
        ComboBox {
            id: periodBox
            Layout.fillWidth: true
            model: ["Каждый день", "Каждую неделю"]
        }
    }

    onAccepted: {
        const n = nameField.text.trim()
        const target = parseFloat(targetField.text)
        if (n === "" || isNaN(target) || target <= 0)
            return
        plannerManager.addHabit(n, target, unitField.text.trim(),
                                periodBox.currentIndex === 1 ? "week" : "day")
    }
}
