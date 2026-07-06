import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dlg
    property string presetDate: Qt.formatDate(new Date(), "yyyy-MM-dd")
    // editId >= 0 — режим редактирования существующего события
    property int editId: -1
    readonly property var repeats: ["", "daily", "weekly", "monthly"]

    function openForEdit(ev, dateStr) {
        editId = ev.id
        open()
        titleField.text = ev.title
        dateField.text = dateStr
        timeField.text = ev.time
        descField.text = ev.description
        const idx = repeats.indexOf(ev.repeat)
        repeatBox.currentIndex = idx < 0 ? 0 : idx
    }

    title: editId >= 0 ? "✏️ Изменить событие" : "📅 Новое событие"
    modal: true
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 380

    onOpened: {
        if (editId < 0) {
            titleField.text = ""
            dateField.text = presetDate
            timeField.text = ""
            descField.text = ""
            repeatBox.currentIndex = 0
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
            placeholderText: "Название"
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: dateField
                Layout.fillWidth: true
                placeholderText: "ГГГГ-ММ-ДД"
            }
            TextField {
                id: timeField
                Layout.fillWidth: true
                placeholderText: "ЧЧ:ММ (необязательно)"
            }
        }
        TextField {
            id: descField
            Layout.fillWidth: true
            placeholderText: "Описание (необязательно)"
        }
        ComboBox {
            id: repeatBox
            Layout.fillWidth: true
            model: ["Не повторять", "Каждый день", "Каждую неделю", "Каждый месяц"]
        }
    }

    onAccepted: {
        const t = titleField.text.trim()
        if (t === "")
            return
        if (editId >= 0)
            plannerManager.updateEvent(editId, t, dateField.text.trim(),
                                       timeField.text.trim(), descField.text.trim(),
                                       repeats[repeatBox.currentIndex])
        else
            plannerManager.addEvent(t, dateField.text.trim(), timeField.text.trim(),
                                    descField.text.trim(), repeats[repeatBox.currentIndex])
    }
}
