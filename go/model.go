package qbackend

type RowGetter interface {
	Row(row int) interface{}
	Rows() []interface{}
}

type Model struct {
	Rows  RowGetter
	Store *Store
}

func (m *Model) Data() interface{} {
	return struct {
		Data interface{} `json:"data"`
	}{m.Rows.Rows()}
}

func (m *Model) Reset() {
	m.Store.Emit("reset", m.Data())
}

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
