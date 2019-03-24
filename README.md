# QBackend

QBackend combines a backend Go application with a QtQuick/QML user interface using a magical seamless API on both sides. It allows you to write a Go application naturally (without boilerplate and weird UI adaptors) and a user interface in QML that does exactly what you'd normally expect it to do.

##### Go Backends
* Go structs become usable as normal objects in QML with one line of code. No initialization necessary.
* Objects have properties, methods, and signals. Feel free to pass around a map of structs with arrays of interfaces containing objects.
* Objects are garbage collected normally when no longer in use from Go and QML
* Really, one line. It's `qbackend.QObject` (embedded in a struct).

##### QML Interfaces
* The entire API: `import Crimson.QBackend 1.0`, `Backend` is a singleton to help get things started.
* Register a Go type factory (also one line), instantiate it declaratively in QML like any other type
* Backends can provide objects that are also QAbstractListModel-style models

##### Flexible
* Socket based and optionally split-process; no cgo, no need to link Qt
* There's also a convenient package to run the backend and QML in one process
* Fairly reasonable options for managing concurrency with other Go code

### How It Looks

###### magic.go
```go
package main

import (
	qbackend "github.com/CrimsonAS/qbackend/backend"
	"github.com/CrimsonAS/qbackend/backend/qmlscene"
)

type Root struct {
	qbackend.QObject
	Message         string
	SomethingHappen func()
}

func (r *Root) SetMessage(msg string) {
	r.Message = msg
	r.Changed("Message")
	r.SomethingHappen() // emit signal
}

func main() {
	qmlscene.Connection().RootObject = &Root{Message: "It's magic"}
	qmlscene.ExecScene("magic.qml")
}
```

###### magic.qml
```qml
import QtQuick 2.9
import QtQuick.Window 2.2
import Crimson.QBackend 1.0

Window {
    width: 600
    height: 400
    visible: true

    Text {
        anchors.centerIn: parent
        font.pixelSize: 24
        text: Backend.message
    }

    TextInput {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        focus: true
        onTextEdited: Backend.message = text
    }

    Connections {
        target: Backend
        onSomethingHappen: console.log("something happened!")
    }
}
```

### Why?

Go is great, but the options for UIs aren't. QtQuick is easy, flexible, and maturing. I enjoy using both of them, and I want to use them together.

[go-qml](https://github.com/go-qml/qml) (and its more recent forks) have existed for years. Its scope is very different, closer to writing Go bindings for QtQuick, and like most bindings the API is somewhere between clunky and broken. I think it's fundamentally the wrong approach, trying to match low level behavior rather than build good high level API. The original package is abandoned and lacked support for things like models or arrays.

Go and Qt both have powerful object reflection, so it's possible to dynamically create types and build essentially native objects. The potential exists for seamless APIs that don't feel like an intrusive hack bridging between languages.

There is interesting potential in multiprocess graphical applications.

### How To Use

TODO: Documentation ;)

Until then, examples and code comments are all you get.

### How It Works

TODO: Design documentation!

A short, dense introduction:

The connection is a socket and JSON-based RPCish protocol between the backend (Go package) and client (QML plugin). Go structs embed `qbackend.QObject`, effectively similar to inheriting QObject in Qt. Starting with a singleton 'root object', these can be reflected to build type definitions, and will be marshaled and sent to the client as-needed. Fields become properties, exported methods are methods, and function type fields are signals. When marshaling objects, any struct encountered with `qbackend.QObject` is automatically initialized and assigned a unique id. QObjects marshal as a type and id reference rather than their full data, which is sent once the client references the object. QObject embeds a handful of useful methods, like `Changed` and `ResetProperties` to signal updates. Internal reference counting tracks which objects the client has or could use, and drops unreferenced objects from its list to allow garbage collection. If those objects are used again instead, nothing will have changed. Overall, you can just treat these as any struct and the right things will happen.

From QML, importing the plugin establishes a connection and registers any instantiable types (uncreateable types do not need any registration). Object types are a JSON description generated from Go reflection, which is translated to a QMetaObject, which effectively _is_ a QObject from QML's point of view. Backend objects are created with an adaptor type using the QMetaObject, which provides all of the metacalls to handle property reads/writes, method calls, and signals. The adaptor object is a reference to the instance from the backend, and will be GC'd when no longer referenced from QML, which allows the backend object to be freed by Go GC. Adaptors are created (and objects referenced) as-needed when an 'object ref' is encountered in data being returned to QML, but initially have a type description with no data. Object data is populated just-in-time when properties of the object are actually used.

Models are just objects that provide a particular set of methods. On the backend, `qbackend.Model` provides an API for this (and is also a QObject). From the client, these are normal objects that also inherit QAbstractListModel.

## Development

TODO: A list of things to do

