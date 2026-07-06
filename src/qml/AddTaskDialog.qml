import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dlg
    // editId >= 0 — режим редактирования существующей задачи
    property int editId: -1

    function openForEdit(task) {
        editId = task.id
        open()
        titleField.text = task.title
        dueField.text = task.dueDate
        priorityBox.currentIndex = task.priority
    }

    title: editId >= 0 ? "✏️ Изменить задачу" : "✅ Новая задача"
    modal: true
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 380

    onOpened: {
        if (editId < 0) {
            titleField.text = ""
            dueField.text = ""
            priorityBox.currentIndex = 1
        }
        titleField.forceActiveFocus()
    }
    onClosed: editId = -1

    ColumnLayout {
        width: parent.width
        spacing: 8

        TextField {
            id: titleField
            Layout.fillWidth: true
            placeholderText: "Что нужно сделать?"
        }
        TextField {
            id: dueField
            Layout.fillWidth: true
            placeholderText: "Срок: ГГГГ-ММ-ДД (необязательно)"
        }
        ComboBox {
            id: priorityBox
            Layout.fillWidth: true
            model: ["🟢 Низкий приоритет", "🟠 Обычный приоритет", "🔴 Высокий приоритет"]
        }
    }

    onAccepted: {
        const t = titleField.text.trim()
        if (t === "")
            return
        if (editId >= 0)
            plannerManager.updateTask(editId, t, dueField.text.trim(),
                                      priorityBox.currentIndex)
        else
            plannerManager.addTask(t, dueField.text.trim(), priorityBox.currentIndex)
    }
}
