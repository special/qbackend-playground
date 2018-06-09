import QtQuick 2.6
import QtQuick.Controls 2.2
import qbackend 1.0

Item {
    width: 500
    height: 800

    property var dataObject: backendProcess.root

    BackendProcess {
        id: backendProcess
        name: "go"
        args: [ "run", "test.go" ]
    }

    /*ListView {
        id: myView
        width: parent.width
        anchors.top: headerRow.bottom
        anchors.bottom: bottomRow.top
        model: BackendJsonListModel {
            id: myData
            connection: backendProcess
            identifier: "PersonModel"
            roles: [ "firstName", "lastName", "age" ]
        }
        delegate: Item {
            height: col.height
            width: ListView.view.width

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    myView.currentIndex = index
                }
            }

            Column {
                id: col
                width: parent.width

                Item {
                    width: parent.width
                    height: childrenRect.height

                    Label { anchors.verticalCenter: removeButt.verticalCenter; text: model.lastName + ", " + model.firstName + ": " + model.age + " years old"}

                    Button {
                        id: removeButt
                        anchors.right: parent.right
                        text: "Remove"

                        onClicked: {
                            myData.invokeMethod("remove", [ model._uuid ])
                        }
                    }
                }

                Column {
                    height: myView.currentIndex == index ? childrenRect.height : 0
                    visible: height > 0 ? true : false
                    width: childrenRect.width
                    clip: true
                    x: 20
                    spacing: 10

                    TextField {
                        id: firstNameText
                        placeholderText: model.firstName
                    }
                    TextField {
                        id: lastNameText
                        placeholderText: model.lastName
                    }
                    TextField {
                        id: ageText
                        placeholderText: model.age
                    }

                    Button {
                        text: "Update"
                        onClicked: {
                            var dat = { firstName: firstNameText.text, lastName: lastNameText.text, age: ageText.text }
                            myData.invokeMethod("set", [ model._uuid, dat ])
                            firstNameText.text = ""
                            lastNameText.text = ""
                            ageText.text = ""
                        }
                    }
                }
            }
        }
    }*/

    Row {
        id: headerRow
        width: parent.width
        height: childrenRect.height
        Rectangle {
            color: "red"
            width: parent.width
            height: 50

            Label {
                color: "white"
                text: {
                    if (dataObject.data) {
                        return dataObject.data.testData + " ; people: " + dataObject.data.totalPeople
                    } else {
                        return "Connecting..."
                    }
                }
                anchors.centerIn: parent
                font.bold: true
                font.pixelSize: headerRow.height/2
            }
        }
    }

    /*Row {
        id: bottomRow
        anchors.bottom: parent.bottom
        width: parent.width

        Button {
            height: childrenRect.height * 1.5
            width: parent.width
            text: "Add"
            onClicked: {
                myData.invokeMethod("addNew", [])
            }
        }
    }*/
}
