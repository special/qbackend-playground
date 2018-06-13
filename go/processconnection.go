package qbackend

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"strconv"
)

// ProcessConnection implements the Connection interface to communicate
// with a frontend parent process over stdin/stdout.
type ProcessConnection struct {
	in         io.ReadCloser
	out        io.WriteCloser
	store      map[string]*Store
	rootObject *Store

	processSignal chan struct{}
	queue         chan []byte
}

func NewProcessConnection(in io.ReadCloser, out io.WriteCloser) *ProcessConnection {
	c := &ProcessConnection{
		in:            in,
		out:           out,
		store:         make(map[string]*Store),
		processSignal: make(chan struct{}, 2),
		queue:         make(chan []byte, 128),
	}
	go c.handle()
	return c
}

func NewStdConnection() *ProcessConnection {
	if os.Stdin == nil {
		panic("Cannot create multiple stdin/stdout ProcessConnection instances")
	}

	// Redirect os.Stdin and os.Stdout to nil and stderr respectively
	in, out := os.Stdin, os.Stdout
	os.Stdin, os.Stdout = nil, os.Stderr

	return NewProcessConnection(in, out)
}

type cmdMessage struct {
	Command string `json:"command"`
}

func (c *ProcessConnection) sendMessage(msg interface{}) {
	buf, err := json.Marshal(msg)
	if err != nil {
		panic("invalid message sent on connection")
	}
	fmt.Fprintf(c.out, "%d %s\n", len(buf), buf)
}

func (c *ProcessConnection) handle() {
	defer close(c.processSignal)
	defer close(c.queue)

	c.sendMessage(struct {
		cmdMessage
		Version int `json:"version"`
	}{cmdMessage{"VERSION"}, 2})

	// Send ROOT
	if c.rootObject == nil {
		panic("no root object for active connection")
	}
	c.rootObject.numSubscribed++

	c.sendMessage(struct {
		cmdMessage
		Identifier string          `json:"identifier"`
		Type       typeDescription `json:"type"`
		Data       interface{}     `json:"data"`
	}{
		cmdMessage{"ROOT"},
		"root",
		c.rootObject.Type(),
		c.rootObject.Value(),
	})

	rd := bufio.NewReader(c.in)
	for {
		sizeStr, _ := rd.ReadString(' ')
		if len(sizeStr) < 2 {
			break
		}

		byteCnt, _ := strconv.ParseInt(sizeStr[:len(sizeStr)-1], 10, 32)
		if byteCnt < 1 {
			break
		}

		blob := make([]byte, byteCnt)
		for p := 0; p < len(blob); {
			if n, _ := rd.Read(blob); n == 0 {
				break
			} else {
				p += n
			}
		}

		// Read the final newline
		if nl, _ := rd.ReadByte(); nl != '\n' {
			break
		}

		// Queue and signal
		c.queue <- blob
		c.processSignal <- struct{}{}
	}
}

func (c *ProcessConnection) Run() error {
	for {
		if _, open := <-c.processSignal; !open {
			return nil
		}
		if err := c.Process(); err != nil {
			// Fatal error
			return err
		}
	}
	return nil
}

func (c *ProcessConnection) Process() error {
	for {
		var data []byte
		select {
		case data = <-c.queue:
		default:
			return nil
		}

		var msg map[string]interface{}
		if err := json.Unmarshal(data, &msg); err != nil {
			return err
		}

		switch msg["command"] {
		case "SUBSCRIBE":
			if store, exists := c.store[msg["identifier"].(string)]; exists {
				store.numSubscribed++
				if store.numSubscribed < 2 {
					c.storeUpdated(store)
				}
			} else {
				// Ignored; store does not exist
			}

		case "UNSUBSCRIBE":
			if store, exists := c.store[msg["identifier"].(string)]; exists {
				if store.numSubscribed > 0 {
					store.numSubscribed--
				}
			}

		case "INVOKE":
			store, exists := c.store[msg["identifier"].(string)]
			if !exists {
				// Ignored; store does not exist
				continue
			}

			params, ok := msg["parameters"].([]interface{})
			if !ok {
				// Parameters are not an array
				continue
			}

			if err := store.Invoke(msg["method"].(string), params); err != nil {
				// Invoke failed
				continue
			}

		default:
			return fmt.Errorf("unknown command %v", msg)
		}
	}

	return nil
}

func (c *ProcessConnection) ProcessSignal() <-chan struct{} {
	return c.processSignal
}

func (c *ProcessConnection) RootObject() *Store {
	return c.rootObject
}

func (c *ProcessConnection) SetRootObject(obj interface{}) {
	if c.rootObject != nil {
		// XXX this is a little rude, maybe
		panic("cannot reset root object on connection")
	}

	c.rootObject, _ = c.NewStore("root", obj)
}

func (c *ProcessConnection) NewStore(name string, data interface{}) (*Store, error) {
	if c.store[name] != nil {
		return nil, errors.New("store name already defined")
	}

	val := &Store{
		Connection: c,
		Name:       name,
		Data:       data,
	}
	c.store[name] = val
	return val, nil
}

func (c *ProcessConnection) Store(name string) *Store {
	return c.store[name]
}

func (c *ProcessConnection) storeUpdated(store *Store) error {
	c.sendMessage(struct {
		cmdMessage
		Identifier string      `json:"identifier"`
		Data       interface{} `json:"data"`
	}{cmdMessage{"OBJECT_CREATE"}, store.Name, store.Value()})
	return nil
}

func (c *ProcessConnection) storeEmit(store *Store, method string, data interface{}) error {
	c.sendMessage(struct {
		cmdMessage
		Identifier string      `json:"identifier"`
		Method     string      `json:"method"`
		Parameters interface{} `json:"parameters"`
	}{cmdMessage{"EMIT"}, store.Name, method, data})
	return nil
}
