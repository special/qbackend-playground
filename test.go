package main

import (
	"fmt"

	"github.com/CrimsonAS/qbackend/go"
)

type Person struct {
	Store *qbackend.Store `json:"-"`

	FirstName string `json:"firstName"`
	LastName  string `json:"lastName"`
	Age       int    `json:"age"`
}

func (p *Person) BeOlder(years int) {
	p.Age += years
	// XXX store updated
	// XXX this bidirectional reference thing is annoying. Something that indexed
	// by ptr would be neat, maybe?
	p.Store.Updated()
}

type generalData struct {
	TestData    string          `json:"testData"`
	TotalPeople int             `json:"totalPeople"`
	MainPerson  *qbackend.Store `json:"mainPerson"`
	PeopleModel *qbackend.Store `json:"peopleModel"`
}

type PersonModel struct {
	People []Person
}

func (pm *PersonModel) Row(row int) interface{} {
	if row < 0 || row >= len(pm.People) {
		return nil
	}
	r := pm.People[row]
	return []interface{}{r.FirstName, r.LastName, r.Age}
}

func (pm *PersonModel) Rows() []interface{} {
	re := make([]interface{}, len(pm.People))
	for i := 0; i < len(pm.People); i++ {
		r := pm.People[i]
		re[i] = []interface{}{r.FirstName, r.LastName, r.Age}
	}
	return re
}

func main() {
	qb := qbackend.NewStdConnection()

	mainPerson := &Person{FirstName: "Robin", LastName: "Burchell", Age: 31}
	mainPerson.Store, _ = qb.NewStore("thatguy", mainPerson)

	gd := &generalData{TestData: "Now connected", TotalPeople: 666, MainPerson: mainPerson.Store}
	qb.SetRootObject(gd)

	pm := &PersonModel{
		People: []Person{
			Person{FirstName: "Robin", LastName: "Burchell", Age: 31},
			Person{FirstName: "Kamilla", LastName: "Bremeraunet", Age: 30},
		},
	}
	model := qbackend.NewModel(pm, qb)
	gd.PeopleModel = model.Store

	qb.Run()
	fmt.Printf("Quitting?\n")
}
