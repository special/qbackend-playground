package qbackend

import (
	"encoding/json"
	"fmt"
	"github.com/satori/go.uuid"
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
	Data              map[uuid.UUID]interface{} `json:"data"`
	name              string
	subscriptionCount int
	SetHook           func(uuid.UUID, interface{}) `json:"-"`
	RemoveHook        func(uuid.UUID)              `json:"-"`
}

func (this *JsonModel) Publish(name string) {
	if this.name != "" {
		panic("Can't publish twice")
	}
	if name == "" {
		panic("Can't publish an empty name")
	}
	this.name = name
	this.Data = make(map[uuid.UUID]interface{})
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

type setCommand struct {
	UUID uuid.UUID   `json:"UUID"`
	Data interface{} `json:"data"`
}

func (this *JsonModel) Set(uuid uuid.UUID, item interface{}) {
	if this.SetHook != nil {
		this.SetHook(uuid, item)
	}
	this.Data[uuid] = item
	if this.subscriptionCount > 0 {
		Emit(this.name, "set", setCommand{UUID: uuid, Data: item})
	}
}

func (this *JsonModel) Get(uuid uuid.UUID) (interface{}, bool) {
	v, ok := this.Data[uuid]
	return v, ok
}

type removeCommand struct {
	UUID uuid.UUID `json:"UUID"`
}

func (this *JsonModel) Remove(uuid uuid.UUID) {
	if this.RemoveHook != nil {
		this.RemoveHook(uuid)
	}
	delete(this.Data, uuid)
	if this.subscriptionCount > 0 {
		Emit(this.name, "remove", removeCommand{UUID: uuid})
	}
}

func (this *JsonModel) Length() int {
	return len(this.Data)
}
