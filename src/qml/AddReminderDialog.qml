import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dlg
    title: "⏰ Новое напоминание"
    modal: true
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 380

    onOpened: {
        titleField.text = ""
        dateField.text = Qt.formatDate(new Date(), "yyyy-MM-dd")
        timeField.text = ""
        placeField.text = ""
        typeBox.currentIndex = 0
        titleField.forceActiveFocus()
    }

    ColumnLayout {
        width: parent.width
        spacing: 8

        TextField {
            id: titleField
            Layout.fillWidth: true
            placeholderText: "О чём напомнить?"
        }

        ComboBox {
            id: typeBox
            Layout.fillWidth: true
            model: ["⏰ По времени", "📍 По месту (нужен телефон с GPS)"]
        }

        RowLayout {
            visible: typeBox.currentIndex === 0
            Layout.fillWidth: true
            TextField {
                id: dateField
                Layout.fillWidth: true
                placeholderText: "ГГГГ-ММ-ДД"
            }
            TextField {
                id: timeField
                Layout.fillWidth: true
                placeholderText: "ЧЧ:ММ"
            }
        }

        TextField {
            id: placeField
            visible: typeBox.currentIndex === 1
            Layout.fillWidth: true
            placeholderText: "Место (например, «магазин Пятёрочка»)"
        }
    }

    onAccepted: {
        const t = titleField.text.trim()
        if (t === "")
            return
        if (typeBox.currentIndex === 0) {
            const time = timeField.text.trim() !== "" ? timeField.text.trim() : "09:00"
            plannerManager.addTimeReminder(t, dateField.text.trim() + "T" + time + ":00")
        } else {
            if (placeField.text.trim() === "")
                return
            plannerManager.addGeoReminder(t, placeField.text.trim(), 0, 0)
        }
    }
}
