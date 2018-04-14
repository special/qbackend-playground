package qbackend

import (
	"encoding/json"
	"fmt"
)

type Objectable interface {
	// Sets up an object with a given known identifier.
	Publish(identifier string)

	// Notifies the Objectable of a remote subscriber.
	Subscribe()
}

func Startup() {
	fmt.Println("VERSION 2")
}

func Create(identifier string, val interface{}) {
	buf, _ := json.Marshal(val)
	fmt.Println(fmt.Sprintf("OBJECT_CREATE %s %d", identifier, len(buf)))
	fmt.Println(fmt.Sprintf("%s", buf))
}

func Emit(identifier string, method string, val interface{}) {
	buf, _ := json.Marshal(val)
	fmt.Println(fmt.Sprintf("EMIT %s %s %d", identifier, method, len(buf)))
	fmt.Println(fmt.Sprintf("%s", buf))
}
