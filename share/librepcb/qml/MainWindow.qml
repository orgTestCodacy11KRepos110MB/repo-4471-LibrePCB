import QtQuick 2.0
import QtQuick.Controls 2.0
import QtQuick.Window 2.0

ApplicationWindow {
    id: window
    width: 1024
    height: 768
    visible: true
    title: "LibrePCB"

    Row {
        Rectangle {
            width: 60
            height: window.height
            color: "#353535"
            visible: true
            Column {
                anchors.fill: parent
                SideButton {checked: true}
                SideButton {}
                SideButton {}
            }
        }
        Rectangle {
            width: 300
            height: window.height
            color: "#4f4f4f"
        }
    }
}
