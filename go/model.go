package qbackend

type RowGetter interface {
	Row(row int) interface{}
	Rows() []interface{}
}

// modelAPI implements the internal qbackend API for model data; see QBackendModel from the plugin
type modelAPI struct {
	Model     *Model   `json:"-"`
	RoleNames []string `json:"roleNames"`

	// Signals
	ModelReset  func(interface{})      `qbackend:"rowData" json:"-"`
	ModelInsert func(int, interface{}) `qbackend:"start,rowData" json:"-"`
	ModelRemove func(int, int)         `qbackend:"start,end" json:"-"`
	ModelMove   func(int, int, int)    `qbackend:"start,end,destination" json:"-"`
	ModelUpdate func(int, interface{}) `qbackend:"row,data" json:"-"`
}

func (m *modelAPI) Reset() {
	m.Model.Reset()
}

type Model struct {
	Rows  RowGetter
	Store *Store

	api      *modelAPI
	apiStore *Store
}

func NewModel(rows RowGetter, c Connection) *Model {
	m := &Model{
		Rows: rows,
		api:  new(modelAPI),
	}

	// XXX Figure out some reasonable behavior for RoleNames
	m.api.Model = m
	m.api.RoleNames = []string{"firstName", "lastName", "age"}

	// XXX Identifier API still needs fixing..
	m.Store, _ = c.NewStore("XXXmodelXXX", m)
	m.apiStore, _ = c.NewStore("XXXmodelDataXXX", m.api)

	return m
}

func (m *Model) Data() interface{} {
	return struct {
		API *Store `json:"_qb_model"`
	}{m.apiStore}
}

func (m *Model) Reset() {
	m.apiStore.Emit("modelReset", []interface{}{m.Rows.Rows()})
}

// XXX None of the rest of these are (re-)implemented properly yet
func (m *Model) Inserted(start, count int) {
	rows := make([]interface{}, count)
	for i := 0; i < count; i++ {
		rows[i] = m.Rows.Row(start + i)
	}
	m.Store.Emit("insert", struct {
		Start int           `json:"start"`
		Rows  []interface{} `json:"rows"`
	}{start, rows})
}

func (m *Model) Removed(start, count int) {
	m.Store.Emit("remove", struct {
		Start int `json:"start"`
		End   int `json:"end"`
	}{start, start + count - 1})
}

func (m *Model) Moved(start, count, destination int) {
	m.Store.Emit("move", struct {
		Start       int `json:"start"`
		End         int `json:"end"`
		Destination int `json:"destination"`
	}{start, start + count - 1, destination})
}

func (m *Model) Updated(row int) {
	rows := make(map[int]interface{})
	rows[row] = m.Rows.Row(row)
	m.Store.Emit("update", struct {
		Rows map[int]interface{}
	}{rows})
}

// Attempt to invoke methods on the Rows object first
func (m *Model) Invoke(method string, args []interface{}) bool {
	err := m.Store.invokeDataObject(m.Rows, method, args)
	return err == nil
}
