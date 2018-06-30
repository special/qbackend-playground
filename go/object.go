package qbackend

import (
	"encoding"
	"encoding/json"
	"errors"
	"fmt"
	"reflect"
	"time"

	uuid "github.com/satori/go.uuid"
)

const (
	objectRefGracePeriod = 5 * time.Second
)

// Add names of any functions in QObject to the blacklist in type.go

// The QObject interface must be embedded in any struct that should export
// a full object (as opposed to simple data).
//
// The QObject will be initialized automatically when qbackend encoding
// encounters the object. It may also be initialized explicitly with
// Connection.InitObject(), which is useful to avoid needing nil checks.
type QObject interface {
	json.Marshaler
	MarshalObject() (map[string]interface{}, error)

	Connection() Connection
	Identifier() string
	// Referenced returns true when there is a client-side reference to
	// this object. When false, all signals are ignored and the object
	// will not be encoded.
	Referenced() bool

	// Invoke calls the named method of the object, converting or
	// unmarshaling parameters as necessary. An error is returned if the
	// method is not invoked, but the return value of the method is
	// ignored.
	Invoke(method string, args ...interface{}) error
	// Emit emits the named signal asynchronously. The signal must be
	// defined within the object and parameters must match exactly.
	Emit(signal string, args ...interface{})
	// ResetProperties is effectively identical to emitting the Changed
	// signal for all properties of the object.
	ResetProperties()
}

// QObjectFor indicates whether a value is a qbackend object, and returns
// the embedded QObject instance if present.
func QObjectFor(obj interface{}) (bool, QObject) {
	v := reflect.ValueOf(obj)
	if v.Kind() != reflect.Ptr {
		return false, nil
	}
	v = v.Elem()
	if !v.IsValid() {
		return false, nil
	}
	if v.Kind() != reflect.Struct {
		return false, nil
	}
	f := v.FieldByName("QObject")
	if !f.IsValid() {
		return false, nil
	}
	if f.Type() != reflect.TypeOf((*QObject)(nil)).Elem() {
		return false, nil
	}
	// Also assert that the field is embedded
	if sField, ok := v.Type().FieldByName("QObject"); !ok || !sField.Anonymous {
		return false, nil
	}
	if re := f.Interface(); re == nil {
		return true, nil
	} else {
		return true, re.(QObject)
	}
}

func objectImplFor(obj interface{}) *objectImpl {
	is, q := QObjectFor(obj)
	if !is || q == nil {
		return nil
	} else {
		return q.(*objectImpl)
	}
}

type objectImpl struct {
	C   Connection
	Id  string
	Ref bool

	Object interface{}
	Type   *typeInfo

	// Number of other objects that have a marshaled reference to this one
	refCount int
	// object id -> count for references to other objects in our properties
	refChildren map[string]int
	// Keep object alive until refGraceTime
	refGraceTime time.Time
}

var errNotQObject = errors.New("Struct does not embed QObject")

func initObject(object interface{}, c Connection) (QObject, error) {
	u, _ := uuid.NewV4()
	return initObjectId(object, c, u.String())
}

func initObjectId(object interface{}, c Connection, id string) (QObject, error) {
	if hasObj, obj := QObjectFor(object); !hasObj {
		return nil, errNotQObject
	} else if obj != nil {
		return obj, nil
	}

	impl := &objectImpl{
		C:           c,
		Id:          id,
		Object:      object,
		refChildren: make(map[string]int),
	}

	if ti, err := parseType(reflect.TypeOf(object)); err != nil {
		return nil, err
	} else {
		impl.Type = ti
	}

	// Write to the QObject embedded field
	reflect.ValueOf(object).Elem().FieldByName("QObject").Set(reflect.ValueOf(impl))

	// Initialize signals
	if err := initSignals(object, impl); err != nil {
		return nil, err
	}

	// Set grace period to stop the object from being removed prematurely
	impl.refsChanged()

	// Register with connection
	if c != nil {
		// XXX This is a bad leak: there's no way to ever remove these objects
		// XXX Assuming type of Connection, but that's feeling reasonable..
		c.(*ProcessConnection).addObject(object.(QObject))
	}

	return impl, nil
}

func initSignals(object interface{}, impl *objectImpl) error {
	v := reflect.ValueOf(object).Elem()

	for i := 0; i < v.NumField(); i++ {
		field := v.Field(i)
		if typeShouldIgnoreField(v.Type().Field(i)) || field.Type().Kind() != reflect.Func || !field.IsNil() {
			continue
		}

		name := typeFieldName(v.Type().Field(i))
		if _, isSignal := impl.Type.Signals[name]; !isSignal {
			continue
		}

		// Build a function to assign as the signal
		f := reflect.MakeFunc(field.Type(), func(args []reflect.Value) []reflect.Value {
			impl.emitReflected(name, args)
			return nil
		})
		field.Set(f)
	}

	return nil
}

// Call after changing o.refCount or o.Ref, or when the grace period should reset
func (o *objectImpl) refsChanged() {
	if !o.Ref && o.refCount < 1 {
		o.refGraceTime = time.Now().Add(objectRefGracePeriod)
	}
}

func (o *objectImpl) Connection() Connection {
	return o.C
}
func (o *objectImpl) Identifier() string {
	return o.Id
}
func (o *objectImpl) Referenced() bool {
	return o.Ref
}

func (o *objectImpl) Invoke(methodName string, inArgs ...interface{}) error {
	if _, exists := o.Type.Methods[methodName]; !exists {
		return errors.New("method does not exist")
	}

	// Reflect to find a method named methodName on object
	dataValue := reflect.ValueOf(o.Object)
	method := typeMethodValueByName(dataValue, methodName)
	if !method.IsValid() {
		return errors.New("method does not exist")
	}
	methodType := method.Type()

	// Build list of arguments
	callArgs := make([]reflect.Value, methodType.NumIn())
	// If the first argument is a ptr to o.Object's type, pass it in
	if len(callArgs) > 0 && methodType.In(0) == reflect.TypeOf(o.Object) {
		inArgs = append([]interface{}{o.Object}, inArgs...)
	}

	if len(inArgs) != methodType.NumIn() {
		return fmt.Errorf("wrong number of arguments for %s; expected %d, provided %d",
			methodName, methodType.NumIn(), len(inArgs))
	}

	umType := reflect.TypeOf((*encoding.TextUnmarshaler)(nil)).Elem()
	for i, inArg := range inArgs {
		argType := methodType.In(i)
		inArgValue := reflect.ValueOf(inArg)
		var callArg reflect.Value

		// Replace references to QObjects with the objects themselves
		if inArgValue.Kind() == reflect.Map && inArgValue.Type().Key().Kind() == reflect.String {
			objV := inArgValue.MapIndex(reflect.ValueOf("_qbackend_"))
			if objV.Kind() == reflect.Interface {
				objV = objV.Elem()
			}
			if objV.Kind() != reflect.String || objV.String() != "object" {
				return fmt.Errorf("qobject argument %d is malformed; object tag is incorrect", i)
			}
			objV = inArgValue.MapIndex(reflect.ValueOf("identifier"))
			if objV.Kind() == reflect.Interface {
				objV = objV.Elem()
			}
			if objV.Kind() != reflect.String {
				return fmt.Errorf("qobject argument %d is malformed; invalid identifier %v", i, objV)
			}

			// Will be nil if the object does not exist
			// Replace the inArgValue so the logic below can handle type matching and conversion
			inArgValue = reflect.ValueOf(o.C.Object(objV.String()))
		}

		// Match types, converting or unmarshaling if possible
		if inArgValue.Type() == argType {
			// Types match
			callArg = inArgValue
		} else if inArgValue.Type().ConvertibleTo(argType) {
			// Convert type directly
			callArg = inArgValue.Convert(argType)
		} else if inArgValue.Kind() == reflect.String {
			// Attempt to unmarshal via TextUnmarshaler, directly or by pointer
			var umArg encoding.TextUnmarshaler
			if argType.Implements(umType) {
				callArg = reflect.Zero(argType)
				umArg = callArg.Interface().(encoding.TextUnmarshaler)
			} else if argTypePtr := reflect.PtrTo(argType); argTypePtr.Implements(umType) {
				callArg = reflect.New(argType)
				umArg = callArg.Interface().(encoding.TextUnmarshaler)
				callArg = callArg.Elem()
			}

			if umArg != nil {
				err := umArg.UnmarshalText([]byte(inArg.(string)))
				if err != nil {
					return fmt.Errorf("wrong type for argument %d to %s; expected %s, unmarshal failed: %s",
						i, methodName, argType.String(), err)
				}
			}
		}

		if callArg.IsValid() {
			callArgs[i] = callArg
		} else {
			return fmt.Errorf("wrong type for argument %d to %s; expected %s, provided %s",
				i, methodName, argType.String(), inArgValue.Type().String())
		}
	}

	// Call the method
	returnValues := method.Call(callArgs)

	// If any of method's return values is an error, return that
	errType := reflect.TypeOf((*error)(nil)).Elem()
	for _, value := range returnValues {
		if value.Type().Implements(errType) {
			return value.Interface().(error)
		}
	}

	return nil
}

func (o *objectImpl) Emit(signal string, args ...interface{}) {
	if !o.Referenced() {
		return
	}
	o.C.(*ProcessConnection).sendEmit(o.Object.(QObject), signal, args)
}

func (o *objectImpl) emitReflected(signal string, args []reflect.Value) {
	var unwrappedArgs []interface{}
	for _, a := range args {
		unwrappedArgs = append(unwrappedArgs, a.Interface())
	}
	o.Emit(signal, unwrappedArgs...)
}

func (o *objectImpl) ResetProperties() {
	if !o.Referenced() {
		return
	}
	o.C.(*ProcessConnection).sendUpdate(o)
}

// Unfortunately, even though this method is embedded onto the object type, it can't
// be used to marshal the object type. The QObject field is not explicitly initialized;
// it's meant to initialize automatically when an object is encountered. That isn't
// possible to do when this MarshalJSON method is called, even if it were embedded as
// a struct instead of an interface.
//
// Even if this object were guaranteed to have been initialized, QObjects do not
// marshal recursively, and there would be no way to prevent this within MarshalJSON.
//
// Instead, MarshalObject handles the correct marshaling of values for object types,
// and this function returns the typeinfo that is appropriate when an object is
// referenced from another object.
func (o *objectImpl) MarshalJSON() ([]byte, error) {
	obj := struct {
		Tag        string    `json:"_qbackend_"`
		Identifier string    `json:"identifier"`
		Type       *typeInfo `json:"type"`
	}{
		"object",
		o.Identifier(),
		o.Type,
	}

	// Marshaling typeinfo for an object resets the refcounting grace period.
	// This ensures that the client has enough time to reference an object from
	// e.g. a signal parameter before it could be cleaned up.
	o.refsChanged()

	return json.Marshal(obj)
}

// As noted above, MarshalJSON can't correctly capture and initialize trees containing
// a QObject. MarshalObject scans the struct to initialize QObjects, then returns a
// map that correctly represents the properties of this object. That map can be passed
// (in-)directly to json.Marshal. Specific differences from JSON marshal are:
//
//   - Fields are filtered and renamed in the same manner as properties in typeinfo
//   - Other json tag options on fields are ignored, including omitempty
//   - Signal fields are ignored; these would break MarshalJSON
//   - Any QObject struct encountered is initialized if necessary
//   - QObject structs do not marshal recursively; they only provide typeinfo
//   - Array, slice, non-QObject struct, and map fields are scanned for QObjects and
//     marshal appropriately
//
// Non-QObject fields will be marshaled normally with json.Marshal.
func (o *objectImpl) MarshalObject() (map[string]interface{}, error) {
	data := make(map[string]interface{})

	value := reflect.Indirect(reflect.ValueOf(o.Object))
	for name, index := range o.Type.propertyFieldIndex {
		field := value.Field(index)
		if refs, err := o.initObjectsUnder(field); err != nil {
			return nil, err
		} else {
			// Zero out all child ref counts
			for k, _ := range o.refChildren {
				o.refChildren[k] = 0
			}

			// Add references from refs
			for _, id := range refs {
				if _, existing := o.refChildren[id]; !existing {
					// Reference to an object that was not referenced before
					if obj := o.C.Object(id); obj != nil {
						objectImplFor(obj).refCount++
						o.refsChanged()
					}
				}
				o.refChildren[id]++
			}

			// Dereference objects that are no longer referenced here
			for k, v := range o.refChildren {
				if v > 0 {
					continue
				}
				delete(o.refChildren, k)
				if obj := o.C.Object(k); obj != nil {
					objectImplFor(obj).refCount--
					o.refsChanged()
				}
			}
		}
		data[name] = field.Interface()
	}

	return data, nil
}

// initObjectsUnder scans a Value for references to any QObject types, and
// initializes these if necessary. This scan is recursive through any types
// other than QObject itself.
//
// This scan also maintains the list of object IDs referenced within this
// object, which is returned here and stored as refChildren.
func (o *objectImpl) initObjectsUnder(v reflect.Value) ([]string, error) {
	for v.Kind() == reflect.Ptr {
		v = reflect.Indirect(v)
		if !v.IsValid() {
			// nil pointer
			return nil, nil
		}
	}

	var refs []string

	switch v.Kind() {
	case reflect.Array:
		fallthrough
	case reflect.Slice:
		elemType := v.Type().Elem()
		if !typeCouldContainQObject(elemType) {
			return nil, nil
		}
		for i := 0; i < v.Len(); i++ {
			if elemRefs, err := o.initObjectsUnder(v.Index(i)); err != nil {
				return nil, err
			} else {
				refs = append(refs, elemRefs...)
			}
		}

	case reflect.Map:
		elemType := v.Type().Elem()
		if !typeCouldContainQObject(elemType) {
			return nil, nil
		}
		for _, key := range v.MapKeys() {
			if elemRefs, err := o.initObjectsUnder(v.MapIndex(key)); err != nil {
				return nil, err
			} else {
				refs = append(refs, elemRefs...)
			}
		}

	case reflect.Interface:
		panic("QObject initialization through interfaces is not implemented yet") // XXX

	case reflect.Struct:
		if newObj, err := initObject(v.Addr().Interface(), o.C); err == nil {
			// Valid QObject, possibly just initialized. Stop recursion here
			refs = append(refs, newObj.Identifier())
			return refs, nil
		} else if err != errNotQObject {
			// Is a QObject, but initialization failed
			return nil, err
		}

		// Not a QObject
		for i := 0; i < v.NumField(); i++ {
			if typeShouldIgnoreField(v.Type().Field(i)) {
				continue
			}
			field := v.Field(i)
			if typeCouldContainQObject(field.Type()) {
				if elemRefs, err := o.initObjectsUnder(field); err != nil {
					return nil, err
				} else {
					refs = append(refs, elemRefs...)
				}
			}
		}
	}

	return refs, nil
}

func typeCouldContainQObject(t reflect.Type) bool {
	for {
		switch t.Kind() {
		case reflect.Array:
			fallthrough
		case reflect.Slice:
			fallthrough
		case reflect.Map:
			fallthrough
		case reflect.Ptr:
			t = t.Elem()
			continue

		case reflect.Struct:
			fallthrough
		case reflect.Interface:
			return true

		default:
			return false
		}
	}
}
