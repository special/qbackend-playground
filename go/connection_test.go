package qbackend

import (
	"io"
	"testing"
)

type Child struct {
	QObject
	Title string
}

type Root struct {
	QObject
	Title string
	Child *Child
}

func TestConnectionInit(t *testing.T) {
	r1, _ := io.Pipe()
	_, w2 := io.Pipe()
	c := NewProcessConnection(r1, w2)

	r := &Root{
		Title: "I am Root",
		Child: &Child{
			Title: "I am Child",
		},
	}
	c.SetRootObject(r)
	root := c.RootObject()
	if root == nil || r.Title != "I am Root" || r.Identifier() != "root" {
		t.Errorf("Root object wasn't set sucessfully: %+v", root)
	}
	if c.Object(r.Identifier()) != r {
		t.Error("Root object wasn't registered successfully")
	}
}
