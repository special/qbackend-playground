package qbackend

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
)

// ProcessConnection implements the Connection interface to communicate
// with a frontend parent process over stdin/stdout.
type ProcessConnection struct {
	in         io.ReadCloser
	out        io.WriteCloser
	store      map[string]*Store
	rootObject *Store

	processSignal chan struct{}
	queue         chan connectionMsg
}

func NewProcessConnection(in io.ReadCloser, out io.WriteCloser) *ProcessConnection {
	c := &ProcessConnection{
		in:            in,
		out:           out,
		store:         make(map[string]*Store),
		processSignal: make(chan struct{}, 2),
		queue:         make(chan connectionMsg, 128),
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

type connectionMsg struct {
	Command string
	Args    []string
	Blob    []byte
}

func (c *ProcessConnection) handle() {
	defer close(c.processSignal)
	defer close(c.queue)
	fmt.Fprintln(c.out, "VERSION 2")

	// Send ROOT
	{
		if c.rootObject == nil {
			panic("no root object for active connection")
		}
		c.rootObject.numSubscribed++

		obj := struct {
			Identifier string      `json:"identifier"`
			Data       interface{} `json:"data"`
		}{"root", c.rootObject.Value()}

		buf, err := json.Marshal(obj)
		if err != nil {
			// XXX handle these failures better
			panic("invalid root object for active connection")
		}
		fmt.Fprintf(c.out, "ROOT %d\n%s\n", len(buf), buf)
	}

	rd := bufio.NewReader(c.in)
	for {
		line, _ := rd.ReadString('\n')
		if len(line) < 1 {
			break
		}
		line = line[:len(line)-1]

		cmd := strings.Split(line, " ")
		if len(line) == 0 || len(cmd) < 1 {
			// Malformed line?
			continue
		}

		// Size of the trailing blob, if any
		byteCnt := int64(0)
		switch cmd[0] {
		case "INVOKE":
			if len(cmd) < 4 {
				// Malformed INVOKE
				continue
			}
			// Read blob
			byteCnt, _ = strconv.ParseInt(cmd[3], 10, 32)
		}

		blob := make([]byte, byteCnt)
		for p := 0; p < len(blob); {
			if n, _ := rd.Read(blob); n == 0 {
				break
			} else {
				p += n
			}
		}

		// Queue and signal
		c.queue <- connectionMsg{Command: cmd[0], Args: cmd[1:], Blob: blob}
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
	var msg connectionMsg
	for {
		select {
		case msg = <-c.queue:
		default:
			return nil
		}

		switch msg.Command {
		case "SUBSCRIBE":
			if len(msg.Args) < 1 {
				break
			}

			if store, exists := c.store[msg.Args[0]]; exists {
				store.numSubscribed++
				if store.numSubscribed < 2 {
					c.storeUpdated(store)
				}
			} else {
				// Ignored; store does not exist
			}

		case "UNSUBSCRIBE":
			if len(msg.Args) < 1 {
				break
			}

			if store, exists := c.store[msg.Args[0]]; exists {
				if store.numSubscribed > 0 {
					store.numSubscribed--
				}
			}

		case "INVOKE":
			if len(msg.Args) < 2 {
				break
			}

			store, exists := c.store[msg.Args[0]]
			if !exists {
				// Ignored; store does not exist
				continue
			}

			// Unmarshal arguments array
			var args []interface{}
			if err := json.Unmarshal(msg.Blob, &args); err != nil {
				// Arguments are not an array
				continue
			}

			if err := store.Invoke(msg.Args[1], args); err != nil {
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
	buf, err := json.Marshal(store.Value())
	if err != nil {
		return err
	}
	fmt.Fprintf(c.out, "OBJECT_CREATE %s %d\n%s\n", store.Name, len(buf), buf)
	return nil
}

func (c *ProcessConnection) storeEmit(store *Store, method string, data interface{}) error {
	buf, err := json.Marshal(data)
	if err != nil {
		return err
	}
	fmt.Fprintf(c.out, "EMIT %s %s %d\n%s\n", store.Name, method, len(buf), buf)
	return nil
}
