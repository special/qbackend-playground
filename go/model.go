package qbackend

type RowGetter interface {
	Row(row int) interface{}
	Rows() []interface{}
}

// modelAPI implements the internal qbackend API for model data; see QBackendModel from the plugin
type modelAPI struct {
	QObject
	Model     *Model   `json:"-"`
	RoleNames []string `json:"roleNames"`

	// Signals
	ModelReset  func(interface{})      `qbackend:"rowData"`
	ModelInsert func(int, interface{}) `qbackend:"start,rowData"`
	ModelRemove func(int, int)         `qbackend:"start,end"`
	ModelMove   func(int, int, int)    `qbackend:"start,end,destination"`
	ModelUpdate func(int, interface{}) `qbackend:"row,data"`
}

func (m *modelAPI) Reset() {
	m.Model.Reset()
}

type Model struct {
	QObject
	Rows RowGetter `json:"-"`
	API  *modelAPI `json:"_qb_model"`
}

func NewModel(rows RowGetter, c Connection) *Model {
	m := &Model{
		Rows: rows,
		API:  new(modelAPI),
	}

	// XXX Figure out some reasonable behavior for RoleNames
	m.API.Model = m
	m.API.RoleNames = []string{"firstName", "lastName", "age"}

	return m
}

func (m *Model) Reset() {
	m.API.Emit("modelReset", m.Rows.Rows())
}

// XXX None of the rest of these are (re-)implemented properly yet
func (m *Model) Inserted(start, count int) {
	rows := make([]interface{}, count)
	for i := 0; i < count; i++ {
		rows[i] = m.Rows.Row(start + i)
	}
	m.API.Emit("insert", struct {
		Start int           `json:"start"`
		Rows  []interface{} `json:"rows"`
	}{start, rows})
}

func (m *Model) Removed(start, count int) {
	m.API.Emit("remove", struct {
		Start int `json:"start"`
		End   int `json:"end"`
	}{start, start + count - 1})
}

func (m *Model) Moved(start, count, destination int) {
	m.API.Emit("move", struct {
		Start       int `json:"start"`
		End         int `json:"end"`
		Destination int `json:"destination"`
	}{start, start + count - 1, destination})
}

func (m *Model) Updated(row int) {
	rows := make(map[int]interface{})
	rows[row] = m.Rows.Row(row)
	m.API.Emit("update", struct {
		Rows map[int]interface{}
	}{rows})
}

// Attempt to invoke methods on the Rows object first
/*func (m *Model) Invoke(method string, args []interface{}) bool {
	err := m.API.invokeDataObject(m.Rows, method, args)
	return err == nil
}*/
