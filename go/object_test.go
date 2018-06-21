package qbackend

import (
	"encoding/json"
	"testing"
)

type BasicStruct struct {
	StringData string
}

type BasicQObject struct {
	QObject

	StringData string
	StructData BasicStruct
	Child      *BasicQObject
}

func TestQObjectInit(t *testing.T) {
	q := &BasicQObject{}
	if isQObject, _ := QObjectFor(q); !isQObject {
		t.Error("QObject struct not detected as QObject")
	}

	// XXX Use a mock connection; nil will eventually fail
	if _, err := initObject(q, nil); err != nil {
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

	// XXX Explicitly initializing because there's no root to work with
	// XXX See above about the Connection
	if _, err := initObject(q, nil); err != nil {
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
	if _, err := initObject(q, nil); err != nil {
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

func TestMethods(t *testing.T) {
	q := &MethodQObject{}

	if _, err := initObject(q, nil); err != nil {
		t.Errorf("QObject initialization failed: %s", err)
	}

	ti, _ := json.Marshal(q.QObject.(*objectImpl).Type)
	t.Logf("Typeinfo: %s", ti)

	err := q.Invoke("Increment")
	if err != nil || q.Count != 1 {
		t.Errorf("Invoking 'Increment' failed: %v", err)
	}

	err = q.Invoke("Add", 4)
	if err != nil || q.Count != 5 {
		t.Errorf("Invoking 'Add' failed: %v", err)
	}
}
