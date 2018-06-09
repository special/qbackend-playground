package qbackend

import (
	"encoding"
	"encoding/json"
	"errors"
	"fmt"
	"reflect"
	"strings"
)

type Store struct {
	Connection Connection
	Name       string
	// Data is an arbitrary encodable value for this Store. The value of Data
	// is provided to all subscribers and updated after calls to Updated().
	//
	// Methods of Data should generally be possible to transparently invoke from
	// the frontend. When a method is invoked on the Store, it will attempt to
	// call the method of Data by the same name with demarshaled arguments,
	// prefering an exact match but also accepting a match with the first letter
	// capitalized.
	//
	// If an invoked method's first argument is a *Store, it will be called with
	// the Store instance. Otherwise, the number of parameters must match exactly,
	// and the unmarshaled types must be convertible to the parameter types,
	// either directly or via encoding.UnmarshalText.
	//
	// When a method is invoked on the Store and the Data implements
	// InvokableStore, the Invoke method is called before any reflection, and
	// reflection does not proceed if that returns true.
	Data interface{}

	numSubscribed int
}

// If a Store.Data is an InvokableStore, the Invoke method is called for any
// INVOKE on this store, before any other processing. If the Invoke method
// returns true, the method call is considered handled. Otherwise, normal
// handling will proceed.
type InvokableStore interface {
	Invoke(method string, args []interface{}) bool
}

// If a Store.Data is a DataFuncStore, the result of the Data method is
// marshalled and transmitted rather than Store.Data itself.
type DataFuncStore interface {
	Data() interface{}
}

// Value returns the value of Data or the result of the DataFuncStore
func (s *Store) Value() interface{} {
	if fs, ok := s.Data.(DataFuncStore); ok {
		return fs.Data()
	}
	return s.Data
}

// Updated signals changes in the store to all subscribers.
func (s *Store) Updated() {
	if s.numSubscribed < 1 {
		return
	}

	s.Connection.storeUpdated(s)
}

// Invoke calls a method of this store on this backend.
func (s *Store) Invoke(methodName string, inArgs []interface{}) error {
	return s.invokeDataObject(s.Data, methodName, inArgs)
}

func (s *Store) invokeDataObject(object interface{}, methodName string, inArgs []interface{}) error {
	if object == nil {
		return errors.New("method invoked on nil object")
	} else if len(methodName) < 1 {
		return errors.New("invoked empty method name")
	}

	// If object is an InvokableStore, try its Invoke method first
	if is, ok := object.(InvokableStore); ok {
		if is.Invoke(methodName, inArgs) {
			return nil
		}
	}

	// Reflect to find a method named methodName on object
	dataValue := reflect.ValueOf(object)
	method := dataValue.MethodByName(methodName)
	if !method.IsValid() {
		// Try an uppercase first letter
		methodName = strings.ToUpper(string(methodName[0])) + methodName[1:]
		if method = dataValue.MethodByName(methodName); !method.IsValid() {
			return errors.New("method does not exist")
		}
	}
	methodType := method.Type()

	// Build list of arguments
	callArgs := make([]reflect.Value, methodType.NumIn())
	// If the first argument is a *Store, pass s
	if len(callArgs) > 0 && methodType.In(0) == reflect.TypeOf(s) {
		inArgs = append([]interface{}{s}, inArgs...)
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

		// Convert types if necessary and possible
		if inArgValue.Type() == argType {
			// Types match
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
				i, methodName, argType.String(), callArg.Type().String())
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

// Emit signals an action to all subscribers of this store.
func (s *Store) Emit(method string, data interface{}) error {
	if s.numSubscribed < 1 {
		return nil
	}

	return s.Connection.storeEmit(s, method, data)
}

func (s *Store) NumSubscribed() int {
	return s.numSubscribed
}

// Generate a qbackend object reference
func (s *Store) MarshalJSON() ([]byte, error) {
	obj := struct {
		Tag        string      `json:"_qbackend_"`
		Identifier string      `json:"identifier"`
		Data       interface{} `json:"data"`
	}{
		"object",
		s.Name,
		s.Data,
	}
	return json.Marshal(obj)
}
