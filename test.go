package main

import (
	qbackend "./qbackend_go"
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"github.com/satori/go.uuid"
	"os"
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
// -> OBJECT_INVOKE <uuid> append 9
// -> (json)
// -> OBJECT_INVOKE <uuid> remove 9
//
// .....
//
// -> OBJECT_INVOKE <uuid> random 9
// ... if it's not a model-internal method, it can then be passed to QML etc to
// handle. imagine:
//
// -> OBJECT_INVOKE <uuid> fileTransferIncoming 9
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
// OBJECT_INVOKE <uuid> addNew
// OBJECT_INVOKE <uuid> update...
//
// all of the logic for these would be left to the object itself.
//
// Messages:
// VERSION 2
// OBJECT_CREATE
// OBJECT_UPDATE
// OBJECT_REGISTER
// OBJECT_DESTROY
// OBJECT_INVOKE

type Person struct {
	UUID      uuid.UUID
	FirstName string `json:"firstName"`
	LastName  string `json:"lastName"`
	Age       int    `json:"age,string"`
}

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
		fmt.Printf("DEBUG Want %d got %d\n", byteCnt, len(data))
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

type generalData struct {
	TestData    string `json:"testData"`
	TotalPeople int    `json:"totalPeople"`
}

type PersonModel struct {
	qbackend.JsonModel
}

func main() {
	qbackend.Startup()

	pm := &PersonModel{}
	pm.Publish("PersonModel")

	gd := generalData{TestData: "Now connected", TotalPeople: pm.Length()}
	pm.Append(Person{FirstName: "Robin", LastName: "Burchell", Age: 31, UUID: uuid.NewV4()})
	pm.Append(Person{FirstName: "Kamilla", LastName: "Bremeraunet", Age: 30, UUID: uuid.NewV4()})

	scanner := bufio.NewScanner(os.Stdin)
	scanner.Split(scanLinesOrBlock)

	for scanner.Scan() {
		line := scanner.Text()
		fmt.Println(fmt.Sprintf("DEBUG %s", line))

		if strings.HasPrefix(line, "SUBSCRIBE ") {
			parts := strings.Split(line, " ")
			if parts[1] == "PersonModel" {
				pm.Subscribe()
			} else if parts[1] == "generalData" {
				qbackend.Create("generalData", gd)
			}
		} else if strings.HasPrefix(line, "INVOKE ") {
			parts := strings.Split(line, " ")
			if len(parts) < 4 {
				fmt.Println(fmt.Sprintf("DEBUG %s too short!", line))
				continue
			}

			// Read the JSON blob
			byteCnt, _ = strconv.ParseInt(parts[3], 10, 32)

			scanningForDataLength = true
			scanner.Scan()
			scanningForDataLength = false

			jsonBlob := []byte(scanner.Text())

			if parts[1] == "PersonModel" {
				if parts[2] == "addNew" {
					pm.Append(Person{FirstName: "Another", LastName: "Person", Age: 15 + pm.Length(), UUID: uuid.NewV4()})
					gd.TotalPeople = pm.Length()
					qbackend.Create("generalData", gd)
				}

				// ### must be a model member for now
				if parts[2] == "remove" {
					type removeCommand struct {
						UUID uuid.UUID `json:"UUID"`
					}
					var removeCmd removeCommand
					json.Unmarshal(jsonBlob, &removeCmd)
					fmt.Printf("Removing %+v (from JSON blob %s)\n", removeCmd, jsonBlob)
					for idx, v := range pm.Data {
						p := v.(*Person)
						if p.UUID == removeCmd.UUID {
							pm.Data = append(pm.Data[0:idx], pm.Data[idx+1:]...)
							qbackend.Create("PersonModel", pm)
							break
						}
					}
				} else if parts[2] == "update" {
					var pobj Person
					json.Unmarshal([]byte(jsonBlob), &pobj)
					for idx, v := range pm.Data {
						p := v.(*Person)
						if p.UUID == pobj.UUID {
							fmt.Printf("DEBUG %+v from %s\n", pobj, jsonBlob)
							pm.Data[idx] = pobj
							qbackend.Create("PersonModel", pm)
							break
						}
					}
				}
			}

			// Skip the JSON blob
		}
	}
}
