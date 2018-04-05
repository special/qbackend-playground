package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"github.com/satori/go.uuid"
	"os"
	"reflect"
	"strconv"
	"strings"
)

// current protocol..
// sent from us:
//
// VERSION 1
// MODEL foo firstName lastName (obsolete now UI has roles its side)
// SYNCED (ui blocks until this point, but this isn't really needed anymore)
//
// APPEND foo 9
// (json data)
// UPDATE foo 9
// (json data)
// REMOVE foo 9
//
//
// sent to us:
//
// INVOKE foo method 9
// (json data)
// -- invoke method on object
//
// OINVOKE foo method id 9
// (json data)
// -- invoke method on object, sub-object
//
////////////////////////////////////////////////////////////////////////////
//
// V2:
//
// -> VERSION 2
// -> OBJECT_CREATE <uuid>
// -> OBJECT_REGISTER <uuid> foo // register the uuid with this name for lookup
// -> SYNC (no blocking needed now, this is just informational, for error checking)
// -> INVOKE <uuid> append 9
// -> (json)
// -> INVOKE <uuid> remove 9
//
// .....
//
// -> INVOKE <uuid> random 9
// ... if it's not a model-internal method, it can then be passed to QML etc to
// handle. imagine:
//
// -> INVOKE <uuid> fileTransferIncoming 9
// (json)
//
// ... for notifications from the backend.
//
// Here, everything is an object. Objects that the system might want to interact
// with as a "top level" item of interest are registered with a name so that
// they might be found: think of models, or settings objects, ... -- we don't
// codify any of their behaviour _at the protocol level_, instead, we just
// define messages to introduce objects, destroy objects, and invoke methods on
// them.
//
// The protocol for UI to backend would be virtually identical:
// INVOKE <uuid> addNew
// INVOKE <uuid> update...
//
// all of the logic for these would be left to the object itself.
//
// Messages:
// VERSION 2
// OBJECT_CREATE
// OBJECT_UPDATE
// OBJECT_REGISTER
// OBJECT_DESTROY
// SYNC
// INVOKE

type Person struct {
	FirstName string `json:"firstName"`
	LastName  string `json:"lastName"`
	Age       int    `json:"age"`
}

func printRoles(v interface{}) {
	val := reflect.ValueOf(v)
	var roles []string

	for i := 0; i < val.Type().NumField(); i++ {
		roles = append(roles, val.Type().Field(i).Tag.Get("json"))
	}

	fmt.Println(fmt.Sprintf("MODEL %T %s", v, strings.Join(roles, " ")))
}

func printPerson(p Person) {
	buf, _ := json.Marshal(p)
	fmt.Println(fmt.Sprintf("APPEND %T %s %d", p, uuid.NewV4(), len(buf)))
	fmt.Println(fmt.Sprintf("%s", buf))
}

var personCount = 0
var scanningForDataLength bool = false
var byteCnt int64

// dropCR drops a terminal \r from the data.
func dropCR(data []byte) []byte {
	if len(data) > 0 && data[len(data)-1] == '\r' {
		return data[0 : len(data)-1]
	}

	return data
}

// Hack to make Scanner give us line by line data, or a block of byteCnt bytes.
func scanLinesOrBlock(data []byte, atEOF bool) (advance int, token []byte, err error) {
	if scanningForDataLength {
		//fmt.Printf("DEBUG read %d bytes wanted %d\n", len(data), byteCnt)
		if len(data) < int(byteCnt) {
			return 0, nil, nil
		}

		return int(byteCnt), data, nil
	}

	if atEOF && len(data) == 0 {
		return 0, nil, nil
	}

	if i := bytes.IndexByte(data, '\n'); i >= 0 {
		// We have a full newline-terminated line.
		return i + 1, dropCR(data[0:i]), nil
	}

	// If we're at EOF, we have a final, non-terminated line. Return it.
	if atEOF {
		return len(data), dropCR(data), nil
	}

	// Request more data.
	return 0, nil, nil

}

func main() {
	fmt.Println("VERSION 1")
	printRoles(Person{})
	fmt.Println("SYNCED")

	printPerson(Person{FirstName: "Robin", LastName: "Burchell", Age: 31})
	printPerson(Person{FirstName: "Kamilla", LastName: "Bremeraunet", Age: 30})

	scanner := bufio.NewScanner(os.Stdin)
	scanner.Split(scanLinesOrBlock)

	for scanner.Scan() {
		line := scanner.Text()
		fmt.Println(fmt.Sprintf("DEBUG %s", line))
		if strings.HasPrefix(line, "INVOKE ") {
			// INVOKE objIdentifier method len
			parts := strings.Split(line, " ")
			if len(parts) < 4 {
				continue
			}

			if parts[1] == "main.Person" {
				if parts[2] == "addNew" {
					printPerson(Person{FirstName: "Another", LastName: "Person", Age: 15 + personCount})
					personCount++
				}
			}

			// Skip the JSON blob
			byteCnt, _ = strconv.ParseInt(parts[3], 10, 32)

			scanningForDataLength = true
			scanner.Scan()
			scanningForDataLength = false
		} else if strings.HasPrefix(line, "OINVOKE ") {
			// INVOKE objIdentifier method uuid len
			parts := strings.Split(line, " ")
			if len(parts) < 5 {
				continue
			}

			// Read the JSON blob
			byteCnt, _ = strconv.ParseInt(parts[3], 10, 32)

			scanningForDataLength = true
			scanner.Scan()
			scanningForDataLength = false

			jsonBlob := scanner.Text()

			if parts[1] == "main.Person" {
				if parts[2] == "remove" {
					fmt.Println(fmt.Sprintf("REMOVE main.Person %s", parts[3]))
				} else if parts[2] == "update" {
					fmt.Println(fmt.Sprintf("UPDATE main.Person %s %d", parts[3], len(jsonBlob)))
					fmt.Println(jsonBlob)
				}
			}
		}
	}
}
