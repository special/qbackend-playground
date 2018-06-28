package qbackend

import (
	"bufio"
	"encoding/json"
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
	objects    map[string]QObject
	rootObject QObject

	processSignal chan struct{}
	queue         chan []byte
}

func NewProcessConnection(in io.ReadCloser, out io.WriteCloser) *ProcessConnection {
	c := &ProcessConnection{
		in:            in,
		out:           out,
		objects:       make(map[string]QObject),
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
	{
		if c.rootObject == nil {
			panic("no root object for active connection")
		}
		objectImplFor(c.rootObject).Ref = true

		data, err := c.rootObject.MarshalObject()
		if err != nil {
			// XXX handle errors..
			panic("root object marshal failed")
		}

		c.sendMessage(struct {
			cmdMessage
			Identifier string      `json:"identifier"`
			Type       *typeInfo   `json:"type"`
			Data       interface{} `json:"data"`
		}{
			cmdMessage{"ROOT"},
			"root",
			objectImplFor(c.rootObject).Type,
			data,
		})
	}

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
		case "OBJECT_REF":
			if obj, exists := c.objects[msg["identifier"].(string)]; exists {
				objectImplFor(obj).Ref = true
			}

		case "OBJECT_DEREF":
			if obj, exists := c.objects[msg["identifier"].(string)]; exists {
				objectImplFor(obj).Ref = false
			}

		case "OBJECT_QUERY":
			if obj, exists := c.objects[msg["identifier"].(string)]; exists {
				c.sendUpdate(obj)
			}

		case "INVOKE":
			obj, exists := c.objects[msg["identifier"].(string)]
			if !exists {
				// Ignored; object does not exist
				continue
			}

			params, ok := msg["parameters"].([]interface{})
			if !ok {
				// Parameters are not an array
				continue
			}

			if err := obj.Invoke(msg["method"].(string), params...); err != nil {
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

func (c *ProcessConnection) RootObject() QObject {
	return c.rootObject
}

func (c *ProcessConnection) SetRootObject(obj QObject) {
	if c.rootObject != nil {
		// XXX this is a little rude, maybe
		panic("cannot reset root object on connection")
	}
	c.rootObject = obj
	if _, err := initObjectId(obj, c, "root"); err != nil {
		// XXX sigh
		panic("initializing root object failed")
	}
}

func (c *ProcessConnection) addObject(obj QObject) {
	id := obj.Identifier()
	if eObj, exists := c.objects[id]; exists {
		if obj == eObj {
			return
		} else {
			// XXX more panic
			panic("Connection has a different object registered with id")
		}
	}

	c.objects[id] = obj
}

func (c *ProcessConnection) Object(name string) QObject {
	return c.objects[name]
}

func (c *ProcessConnection) InitObject(obj QObject) error {
	_, err := initObject(obj, c)
	return err
}

func (c *ProcessConnection) sendUpdate(obj QObject) error {
	if !obj.Referenced() {
		return nil
	}

	data, err := obj.MarshalObject()
	if err != nil {
		return err
	}

	c.sendMessage(struct {
		cmdMessage
		Identifier string                 `json:"identifier"`
		Data       map[string]interface{} `json:"data"`
	}{
		cmdMessage{"OBJECT_RESET"},
		obj.Identifier(),
		data,
	})
	return nil
}

func (c *ProcessConnection) sendEmit(obj QObject, method string, data []interface{}) error {
	c.sendMessage(struct {
		cmdMessage
		Identifier string        `json:"identifier"`
		Method     string        `json:"method"`
		Parameters []interface{} `json:"parameters"`
	}{cmdMessage{"EMIT"}, obj.Identifier(), method, data})
	return nil
}
