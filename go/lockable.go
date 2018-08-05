package qbackend

import "sync"

type channelLocker struct {
	L chan struct{}
	U chan struct{}
}

func newChannelLocker() *channelLocker {
	return &channelLocker{
		L: make(chan struct{}),
		U: make(chan struct{}),
	}
}

func (cl *channelLocker) Lock() {
	cl.L <- struct{}{}
}

func (cl *channelLocker) Unlock() {
	cl.U <- struct{}{}
}

// RunLockable executes Run() in a separate goroutine and returns a sync.Locker, which
// can be used for mutually exclusive execution with Process(). That is, locking
// guarantees that Process() is not and will not run until unlocked.
//
// As noted on Process(), application data is only accessed during calls to Process or
// other qbackend methods. Objects can be safely modified while holding this lock.
// Other methods of Connection and QObject can be used while holding the lock.
//
// RunLockable also returns a channel, which will receive one error value and close
// when the connection is closed.
func (c *Connection) RunLockable() (sync.Locker, <-chan error) {
	lock := newChannelLocker()
	errChannel := make(chan error, 1)

	c.ensureHandler()
	go func() {
		defer close(errChannel)
		for {
			select {
			case _, open := <-c.processSignal:
				if !open {
					errChannel <- c.err
					return
				} else if err := c.Process(); err != nil {
					errChannel <- err
					return
				}
			case <-lock.L:
				<-lock.U
			}
		}
	}()

	return lock, errChannel
}
