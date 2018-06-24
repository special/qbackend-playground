package qbackend

import (
	"encoding/json"
	"fmt"
	"io"
	"os"
	"testing"
)

var dummyConnection *ProcessConnection

type BasicStruct struct {
	StringData string
}

type BasicQObject struct {
	QObject

	StringData string
	StructData BasicStruct
	Child      *BasicQObject
}

func TestMain(m *testing.M) {
	r1, _ := io.Pipe()
	_, w2 := io.Pipe()
	dummyConnection = NewProcessConnection(r1, w2)

	os.Exit(m.Run())
}

func TestQObjectInit(t *testing.T) {
	q := &BasicQObject{}
	if isQObject, _ := QObjectFor(q); !isQObject {
		t.Error("QObject struct not detected as QObject")
	}

	if err := dummyConnection.InitObject(q); err != nil {
		t.Errorf("QObject initialization failed: %s", err)
	}

	if q.QObject == nil {
		t.Error("Embedded QObject still nil after initialization")
	}

	// XXX Identifier uniqueness
	// XXX Signal initialization

	ti, _ := json.Marshal(q.QObject.(*objectImpl).Type)
	t.Logf("Typeinfo: %s", ti)
}

func TestMarshal(t *testing.T) {
	q := &BasicQObject{
		StringData: "hello world",
		StructData: BasicStruct{"hello struct"},
		Child: &BasicQObject{
			StringData: "hello child",
		},
	}

	if err := dummyConnection.InitObject(q); err != nil {
		t.Errorf("QObject initialization failed: %s", err)
	}

	data, err := q.MarshalObject()
	if err != nil {
		t.Errorf("QObject marshal failed: %s", err)
	}
	jsonData, err := json.Marshal(data)
	if err != nil {
		t.Errorf("JSON marshal failed: %s", err)
	}

	t.Logf("Marshaled object: %s", jsonData)
}

type SignalQObject struct {
	QObject
	NoArgs     func()
	NormalArgs func([]int, string) `qbackend:"ints,str"`
	ObjectArgs func(*BasicQObject) `qbackend:"obj"`
}

func TestSignals(t *testing.T) {
	q := &SignalQObject{}

	// Init should assign functions for each signal
	if err := dummyConnection.InitObject(q); err != nil {
		t.Errorf("QObject initialization failed: %s", err)
	}
	if q.NoArgs == nil || q.NormalArgs == nil || q.ObjectArgs == nil {
		t.Errorf("QObject initialization didn't initialize signals: %+v", q)
	}

	ti, _ := json.Marshal(q.QObject.(*objectImpl).Type)
	t.Logf("Typeinfo: %s", ti)

	// Emit signals
	q.NoArgs()
	q.NormalArgs([]int{1, 2, 3, 4, 5}, "one to five")
	q.ObjectArgs(&BasicQObject{StringData: "i am object argument"})
}

type MethodQObject struct {
	QObject
	Count int
}

func (m *MethodQObject) Increment() {
	m.Count++
}

func (m *MethodQObject) Add(i int) {
	m.Count += i
}

func (m *MethodQObject) Update(obj *BasicQObject) {
	if obj != nil {
		obj.StringData = fmt.Sprintf("Count is %d", m.Count)
	}
}

func TestMethods(t *testing.T) {
	q := &MethodQObject{}

	if err := dummyConnection.InitObject(q); err != nil {
		t.Errorf("QObject initialization failed: %s", err)
	}

	ti, _ := json.Marshal(q.QObject.(*objectImpl).Type)
	t.Logf("Typeinfo: %s", ti)

	err := q.Invoke("increment")
	if err != nil || q.Count != 1 {
		t.Errorf("Invoking 'Increment' failed: %v", err)
	}

	err = q.Invoke("add", 4)
	if err != nil || q.Count != 5 {
		t.Errorf("Invoking 'Add' failed: %v", err)
	}

	strObj := &BasicQObject{}
	if err := dummyConnection.InitObject(strObj); err != nil {
		t.Errorf("Initializing object failed: %v", err)
	}

	// There's generally no reason to refer objects this way from Go, so
	// fake the API a little bit.
	strObjRef := make(map[string]string)
	strObjRef["_qbackend_"] = "object"
	strObjRef["identifier"] = strObj.Identifier()
	if err := q.Invoke("update", strObjRef); err != nil {
		t.Errorf("Invoking 'Update' failed: %v", err)
	}
	if strObj.StringData != "Count is 5" {
		t.Error("Object passed as parameter was not modified")
	}
}
