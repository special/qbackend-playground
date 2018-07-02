package qbackend

// Model is embedded in another type instead of QObject to create
// a data model, represented as a QAbstractItemModel to the client.
//
// To be a model, a type must embed Model and must implement the
// ModelDataSource interface. No other special initialization is
// necessary.
//
// When data changes, you must call Model's methods to notify the
// client of the change.
type Model struct {
	QObject
	// ModelAPI is an internal object for the model data API
	ModelAPI *modelAPI `json:"_qb_model"`
}

// Types embedding Model must implement ModelDataSource to provide data
type ModelDataSource interface {
	Row(row int) interface{}
	RowCount() int
	RoleNames() []string
}

// Types embedding Model _may_ implement ModelDataSourceRows to provide
// a list of all rows more efficiently.
//
// If the implementation of Rows() requires copying to a new slice, it may
// be more efficient to not implement this function.
type ModelDataSourceRows interface {
	ModelDataSource
	Rows() []interface{}
}

// modelAPI implements the internal qbackend API for model data; see QBackendModel from the plugin
type modelAPI struct {
	QObject
	Model     *Model   `json:"-"`
	RoleNames []string `json:"roleNames"`

	// Signals
	ModelReset  func([]interface{})      `qbackend:"rowData"`
	ModelInsert func(int, []interface{}) `qbackend:"start,rowData"`
	ModelRemove func(int, int)           `qbackend:"start,end"`
	ModelMove   func(int, int, int)      `qbackend:"start,end,destination"`
	ModelUpdate func(int, interface{})   `qbackend:"row,data"`
}

func (m *modelAPI) Reset() {
	m.Model.Reset()
}

func (m *Model) dataSource() ModelDataSource {
	// The QObject interface is embedded in Model, so it can be accessed from here,
	// but Model is embedded in the app's model type as well, and that is the type
	// that is initialized for the QObject.
	//
	// This enables a neat trick: we can access the QObject here, and its Object
	// field will point back to the app's type, which is usually not available
	// from embedded types.
	impl := objectImplFor(m)
	if impl == nil {
		return nil
	}

	if ds, ok := impl.Object.(ModelDataSource); ok {
		return ds
	} else {
		// XXX Object must implement ModelRowSource; warn/error/panic/something
		return nil
	}
}

func (m *Model) InitObject() {
	data := m.dataSource()

	m.ModelAPI = &modelAPI{
		Model:     m,
		RoleNames: data.RoleNames(),
	}
}

func (m *Model) Reset() {
	data := m.dataSource()
	if data == nil {
		// No-op for uninitialized objects
		return
	}

	if s, ok := data.(ModelDataSourceRows); ok {
		m.ModelAPI.Emit("modelReset", s.Rows())
	} else {
		rows := make([]interface{}, data.RowCount())
		for i := 0; i < len(rows); i++ {
			rows[i] = data.Row(i)
		}
		m.ModelAPI.Emit("modelReset", rows)
	}
}

func (m *Model) Inserted(start, count int) {
	data := m.dataSource()
	if data == nil {
		// No-op for uninitialized objects
		return
	}

	rows := make([]interface{}, count)
	if s, ok := data.(ModelDataSourceRows); ok {
		rows = s.Rows()[start : start+count]
	} else {
		for i := 0; i < count; i++ {
			rows[i] = data.Row(start + i)
		}
	}

	m.ModelAPI.Emit("modelInsert", start, rows)
}

func (m *Model) Removed(start, count int) {
	m.ModelAPI.Emit("modelRemove", start, start+count-1)
}

func (m *Model) Moved(start, count, destination int) {
	m.ModelAPI.Emit("modelMove", start, start+count-1, destination)
}

func (m *Model) Updated(row int) {
	data := m.dataSource()
	if data == nil {
		// No-op for uninitialized objects
		return
	}

	m.ModelAPI.Emit("modelUpdate", row, data.Row(row))
}
