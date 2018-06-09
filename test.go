package main

import (
	"fmt"

	"github.com/CrimsonAS/qbackend/go"
)

type Person struct {
	FirstName string `json:"firstName"`
	LastName  string `json:"lastName"`
	Age       int    `json:"age,string"`
}

type generalData struct {
	TestData    string `json:"testData"`
	TotalPeople int    `json:"totalPeople"`
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
	qb.SetRootObject(&generalData{TestData: "Now connected", TotalPeople: 666})

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
