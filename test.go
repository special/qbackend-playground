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

type Person struct {
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
	pm.Set(uuid.NewV4(), Person{FirstName: "Robin", LastName: "Burchell", Age: 31})
	pm.Set(uuid.NewV4(), Person{FirstName: "Kamilla", LastName: "Bremeraunet", Age: 30})

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
					pm.Set(uuid.NewV4(), Person{FirstName: "Another", LastName: "Person", Age: 15 + pm.Length()})
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
					pm.Remove(removeCmd.UUID)
				} else if parts[2] == "update" {
					type updateCommand struct {
						UUID   uuid.UUID `json:"UUID"`
						Person Person    `json:"data"`
					}
					var updateCmd updateCommand
					err := json.Unmarshal([]byte(jsonBlob), &updateCmd)
					fmt.Printf("From blob %s, person is now %+v err %+v\n", jsonBlob, updateCmd.Person, err)
					pm.Set(updateCmd.UUID, updateCmd.Person)
				}
			}

			// Skip the JSON blob
		}
	}

	fmt.Printf("Quitting?\n")
}
