import QtQuick 2.12;
import QtQuick.Window 2.12;
import main 1.0;

Flickable {


    //contentWidth: Math.max(scene.width * scene.scale, root.width);
    //contentHeight: Math.max(scene.height * scene.scale, root.height);

    //anchors.fill: parent;

    Scene {
        id: scene;
        objectName: "scene";


        //width: 10;
        //height: 10;
        anchors.centerIn: parent;

scale: 0.6

        transformOrigin: Item.TopLeft
        //scale: Math.min(root.width, root.height, 1) + zoom
    }
}
