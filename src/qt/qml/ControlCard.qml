import QtQuick
import QtQuick.Controls.Basic

Frame {
    id: card
    padding: 16

    background: Rectangle {
        color: card.palette.base
        border.color: card.palette.mid
        radius: 12
        Accessible.ignored: true
    }
}
