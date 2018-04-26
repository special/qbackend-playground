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
	// Process or other qbackend methods (such as those in Store), so this gives applications
	// more control over concurrency.
	ProcessSignal() <-chan struct{}

	NewStore(name string, data interface{}) (*Store, error)
	Store(name string) *Store

	storeUpdated(store *Store) error
	storeEmit(store *Store, method string, data interface{}) error
}
