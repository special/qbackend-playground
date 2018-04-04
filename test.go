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
		fmt.Printf("DEBUG read %d bytes wanted %d\n", len(data), byteCnt)
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
		if line == "ADDNEW" {
			printPerson(Person{FirstName: "Another", LastName: "Person", Age: 15 + personCount})
			personCount++
		} else if strings.HasPrefix(line, "REMOVE ") {
			fmt.Println(fmt.Sprintf("REMOVE %s", line[7:]))
		} else if strings.HasPrefix(line, "UPDATE ") {
			// Find the number of bytes we want
			parts := strings.Split(line, " ")
			byteCnt, _ = strconv.ParseInt(parts[2], 10, 32)

			scanningForDataLength = true
			scanner.Scan()
			scanningForDataLength = false

			msg := scanner.Text()

			fmt.Println(fmt.Sprintf("UPDATE %s", line[7:]))
			fmt.Println(fmt.Sprintf("%s", msg))
		}
		fmt.Println(fmt.Sprintf("DEBUG %s", line))
	}
}
