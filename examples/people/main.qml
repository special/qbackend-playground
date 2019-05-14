import QtQuick 2.6
import QtQuick.Window 2.0
import QtQuick.Controls 2.2
import Crimson.QBackend 1.0

Window {
    width: 500
    height: 800
    visible: true

    PersonModel {
        id: personModel
    }

    ListView {
        id: myView
        width: parent.width
        anchors.top: headerRow.bottom
        anchors.bottom: bottomRow.top
        model: personModel
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
                            personModel.removePerson(index)
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
                            personModel.updatePerson(index, firstNameText.text, lastNameText.text, ageText.text)
                            firstNameText.text = ""
                            lastNameText.text = ""
                            ageText.text = ""
                        }
                    }
                }
            }
        }
    }

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
                text: "people: " + personModel.count
                anchors.centerIn: parent
                font.bold: true
                font.pixelSize: headerRow.height/2
            }
        }
    }

    Row {
        id: bottomRow
        anchors.bottom: parent.bottom
        width: parent.width

        Button {
            height: implicitHeight * 2
            width: parent.width
            text: "Add"
            onClicked: {
                personModel.addPerson()
            }
        }
    }
}

