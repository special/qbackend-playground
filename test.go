package main

import (
	"fmt"

	"github.com/CrimsonAS/qbackend/go"
)

type Person struct {
	qbackend.QObject
	FirstName string `json:"firstName"`
	LastName  string `json:"lastName"`
	Age       int    `json:"age"`
}

func (p *Person) BeOlder(years int) {
	p.Age += years
	// XXX store updated
	//p.Store.Updated()
}

type generalData struct {
	qbackend.QObject
	TestData    string          `json:"testData"`
	TotalPeople int             `json:"totalPeople"`
	MainPerson  *Person         `json:"mainPerson"`
	PeopleModel *qbackend.Model `json:"peopleModel"`
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
	gd := &generalData{TestData: "Now connected", TotalPeople: 666, MainPerson: mainPerson}
	qb.SetRootObject(gd)

	pm := &PersonModel{
		People: []Person{
			Person{FirstName: "Robin", LastName: "Burchell", Age: 31},
			Person{FirstName: "Kamilla", LastName: "Bremeraunet", Age: 30},
		},
	}
	gd.PeopleModel = qbackend.NewModel(pm, qb)

	qb.Run()
	fmt.Printf("Quitting?\n")
}
