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
	c := NewConnectionSplit(r1, w2)

	r := &Root{
		Title: "I am Root",
		Child: &Child{
			Title: "I am Child",
		},
	}
	c.RootObject = r
}
