package qbackend

type Connection interface {
	// Run reads and handles messages on the connection until the connection is closed.
	// Using Run is equivalent to looping with Process and ProcessSignal, but requires the
	// application to manage concurrency issues with the data in other ways.
	Run() error

	// Process handles any pending messages on the connection, but does not block to wait
	// for new messages. Combined with ProcessSignal, this method provides a way for
	// applications to control when data is accessed, and to multiplex qbackend handling
	// with other channels.
	//
	// Process returns nil when no messages are pending on the connection. Errors are fatal
	// to the connection.
	Process() error

	// ProcessSignal returns a channel which will be signalled whenever the Connection needs
	// to process data. The caller must call Process() after reading from this channel. The
	// channel is closed when the connection ends.
	//
	// No application data will be modified or accessed except during application calls to
	// Process or other qbackend methods, so this gives applications more control over
	// concurrency.
	ProcessSignal() <-chan struct{}

	RootObject() QObject
	SetRootObject(object QObject)

	Object(identifier string) QObject
	// Explicitly initialize a QObject, assigning an identifier and signal handlers.
	//
	// It's generally not necessary to InitObject manually; objects are initialized as they
	// are encountered in properties and parameters.
	//
	// InitObject can be useful to guarantee that the QObject (and signals) are always
	// usable.
	InitObject(object QObject) error

	RegisterInstantiableType(name string, t QObject, factory func() QObject) error
}
