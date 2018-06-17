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
}

// PersonModel wraps a DataModel to add additional invokable methods
/*type PersonModel struct {
	qbackend.DataModel
}

func (pm *PersonModel) AddNew() {
	pm.Add(Person{FirstName: "Another", LastName: "Person", Age: 15 + pm.Count()})
}*/

func main() {
	qb := qbackend.NewStdConnection()

	mainPerson := &Person{FirstName: "Robin", LastName: "Burchell", Age: 31}
	mainPerson.Store, _ = qb.NewStore("thatguy", mainPerson)

	qb.SetRootObject(&generalData{TestData: "Now connected", TotalPeople: 666, MainPerson: mainPerson.Store})

	//gd := &generalData{TestData: "Now connected", TotalPeople: 666}
	///*gds, _ :=*/ qb.NewStore("GeneralData", gd)

	/*pm := &PersonModel{}
	pm.SetHook = func(uuid uuid.UUID, value interface{}) bool {
		_, existed := pm.Get(uuid)
		if !existed {
			gd.TotalPeople++
			gds.Updated()
		}
		return true
	}
	pm.RemoveHook = func(uuid uuid.UUID) bool {
		gd.TotalPeople--
		gds.Updated()
		return true
	}
	pm.Store, _ = qb.NewStore("PersonModel", pm)

	pm.Add(Person{FirstName: "Robin", LastName: "Burchell", Age: 31})
	pm.Add(Person{FirstName: "Kamilla", LastName: "Bremeraunet", Age: 30})*/

	qb.Run()
	fmt.Printf("Quitting?\n")
}
