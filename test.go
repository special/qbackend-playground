package main

import (
	"fmt"

	"github.com/CrimsonAS/qbackend/backend"
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
	TestData    string
	TotalPeople int
	MainPerson  *Person
	PeopleModel *PersonModel

	TestDataChanged func()
}

func (gd *generalData) PassObject(person *Person) {
	if person == nil {
		gd.TestData = "passed nil person"
	} else {
		gd.TestData = fmt.Sprintf("passed %s %s", person.FirstName, person.LastName)
	}
	gd.TestDataChanged()
	gd.ResetProperties()
}

type PersonModel struct {
	qbackend.Model
	people []Person
}

func (pm *PersonModel) Row(row int) interface{} {
	if row < 0 || row >= len(pm.people) {
		return nil
	}
	r := pm.people[row]
	return []interface{}{r.FirstName, r.LastName, r.Age}
}

func (pm *PersonModel) RowCount() int {
	return len(pm.people)
}

// XXX, heh, could have a type that uses structs for rows and maps the role names to
// field names automatically... would make for very pleasant API
//
// Could do that somewhat automatically even: Row normally returns a []interface{}. If
// it's a struct (ptr) or map instead of a slice... only question then is how to get
// the role names initially, when there may be no rows to give us the type.
func (pm *PersonModel) RoleNames() []string {
	return []string{"firstName", "lastName", "age"}
}

func main() {
	qb := qbackend.NewStdConnection()

	mainPerson := &Person{FirstName: "Robin", LastName: "Burchell", Age: 31}
	gd := &generalData{TestData: "Now connected", TotalPeople: 666, MainPerson: mainPerson}
	qb.SetRootObject(gd)

	gd.PeopleModel = &PersonModel{
		people: []Person{
			Person{FirstName: "Robin", LastName: "Burchell", Age: 31},
			Person{FirstName: "Kamilla", LastName: "Bremeraunet", Age: 30},
		},
	}

	qb.Run()
	fmt.Printf("Quitting?\n")
}
