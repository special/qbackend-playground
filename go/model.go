package qbackend

import (
	uuid "github.com/satori/go.uuid"
)

// Model is a common interface for Store types which can be used as data
// models by the frontend.
type Model interface {
	Data() interface{}
	Count() int
	Get(uuid uuid.UUID) interface{}
	Set(uuid uuid.UUID, data interface{})
	Remove(uuid uuid.UUID)
}

// DataModel is a generic implementation of Model.
type DataModel struct {
	Store *Store
	data  map[uuid.UUID]interface{}

	SetHook    func(uuid uuid.UUID, value interface{}) bool
	RemoveHook func(uuid uuid.UUID) bool
}

func (dm *DataModel) Data() interface{} {
	return struct {
		Data interface{} `json:"data"`
	}{dm.data}
}

func (dm *DataModel) Count() int {
	return len(dm.data)
}

func (dm *DataModel) Get(u uuid.UUID) (interface{}, bool) {
	if dm.data == nil {
		return nil, false
	}

	v, e := dm.data[u]
	return v, e
}

func (dm *DataModel) Set(u uuid.UUID, value interface{}) {
	if dm.SetHook != nil && !dm.SetHook(u, value) {
		return
	}

	if dm.data == nil {
		dm.data = make(map[uuid.UUID]interface{})
	}

	dm.data[u] = value
	dm.Store.Emit("set", struct {
		UUID uuid.UUID
		Data interface{} `json:"data"`
	}{u, value})
}

func (dm *DataModel) Remove(u uuid.UUID) {
	if dm.RemoveHook != nil && !dm.RemoveHook(u) {
		return
	}

	if dm.data == nil {
		return
	}

	delete(dm.data, u)
	dm.Store.Emit("remove", struct {
		UUID uuid.UUID
	}{u})
}
