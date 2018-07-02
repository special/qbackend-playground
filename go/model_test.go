package qbackend

import (
	"fmt"
	"testing"
)

type CustomModel struct {
	Model
}

func (m *CustomModel) Row(row int) interface{} {
	return fmt.Sprintf("row %d", row)
}

func (m *CustomModel) RowCount() int {
	return 3
}

func (m *CustomModel) RoleNames() []string {
	return []string{"text"}
}

var _ ModelDataSource = &CustomModel{}

// Tests
func TestModelType(t *testing.T) {
	model := &CustomModel{}
	if isQObject, _ := QObjectFor(model); !isQObject {
		t.Error("CustomModel type is not detected as a QObject")
	}

	if err := dummyConnection.InitObject(model); err != nil {
		t.Errorf("CustomModel object initialization failed: %s", err)
	}

	impl := objectImplFor(model)
	if impl.Object != model {
		t.Errorf("CustomModel QObject does not point back to model; expected %v, Object is %v", model, impl.Object)
	}

	if model.ModelAPI == nil {
		t.Error("ModelAPI field not initialized during QObject initialization")
	}

	if model.ModelAPI.RoleNames[0] != "text" {
		t.Error("RoleNames not initialized during QObject initialization")
	}
}
