// QBackend combines a backend Go application with a QtQuick/QML user interface using a magical seamless API.
//
// This package establishes a connection to the clientside QML plugin and makes Go structs available as objects
// within QML, complete with properties, methods, and signals. As much as possible, qbackend aims to stay out
// of the way and let you write normal Go and normal QML that somehow work together.
//
// Objects
//
// In the middle of everything is QObject. When QObject is embedded in a struct, that type is "a QObject" and
// will be seen as a fully functional Qt object from the client. Exported fields are available as properties,
// exported methods as object methods, and func fields become signals. Properties and parameters can contain
// other QObjects, structs, maps, arrays, or any encodeable type. QObject adds a few useful methods, such as
// signalling changes to a property.
//
// QObjects are used simply by giving them to the client within the properties and signals of other objects.
// No initialization or registration is necessary, and objects are garbage collected normally when they're not
// referenced in the backend or client. There is no need to treat them differently from any other type.
//
// Connection
//
// Connection handles all communication with the client over an arbitrary socket. Before starting a connection,
// you must set a RootObject, which is a singleton QObject always available to the client -- a useful starting
// point to write the application's API. Connection also registers factories for instantiable types, which can
// be created declaratively like any other QML type.
//
// There are no particular requirements for the connection socket. The QML plugin connects immediately when
// imported using the URL in the root context qbackendUrl property, the -qbackend commandline argument, or
// the QBACKEND_URL environment variable, in that order. The only currently supported scheme is "fd:n[,m]" for
// a r/w socket fd or split read and write sockets.
//
// QBackend guarantees that QObjects are not accessed outside of calls to methods of QObject or Connection.
// Using Connection.Process or Connection.RunLockable, the application can control when Connection processes
// messages and guarantee mutually exclusive execution. Applications with multiple goroutines using QObject
// instances should consider using these for safe concurrency.
//
// Executing QML
//
// The choice of how to manage executing the backend and QML client is up to the application. They can be
// separate processes or a single Go process, they can execute together or rely on a daemon, and the client
// or backend could be executed first. QBackend could use more convenient tools for managing this process.
//
// For applications that want to simply run as a Go binary and execute QML in-process, the backend/qmlscene
// package provides a convenient wrapper for qbackend and https://github.com/special/qgoscene. This makes it
// possible to set up an application with only a few lines of code.
package qbackend
