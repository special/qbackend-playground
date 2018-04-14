package qbackend

import (
	"encoding/json"
	"fmt"
)

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

type JsonModel struct {
	Data              []interface{} `json:"data"`
	name              string
	subscriptionCount int
}

func (this *JsonModel) Publish(name string) {
	if this.name != "" {
		panic("Can't publish twice")
	}
	if name == "" {
		panic("Can't publish an empty name")
	}
	this.name = name
}

func (this *JsonModel) Subscribe() {
	if this.name == "" {
		panic("Can't subscribe an unpublished model")
	}
	if this.subscriptionCount == 0 {
		Create(this.name, this)
	}
	this.subscriptionCount += 1
}

func (this *JsonModel) Append(item interface{}) {
	this.Data = append(this.Data, item)
	if this.subscriptionCount > 0 {
		Emit(this.name, "append", item)
	}
}

func (this *JsonModel) Length() int {
	return len(this.Data)
}
