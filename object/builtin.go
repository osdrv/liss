package object

import (
	"fmt"
	"reflect"
)

var (
	ErrNotAFunction = fmt.Errorf("not a function")
)

var (
	builtins = map[string]any{
		"len":    BuiltinLen,
		"first":  BuiltinFirst,
		"last":   BuiltinLast,
		"rest":   BuiltinRest,
		"isNull": BuiltinIsNull,
	}
)

var Builtins map[string]*BuiltinFunction

func init() {
	for name, fn := range builtins {
		builtinFn, err := NewBuiltinFunction(name, fn)
		if err != nil {
			panic(fmt.Sprintf("failed to create builtin function %s: %s", name, err))
		}
		if Builtins == nil {
			Builtins = make(map[string]*BuiltinFunction)
		}
		Builtins[name] = builtinFn
	}
}

type BuiltinFunction struct {
	name string
	fn   any
	argc int
}

func NewBuiltinFunction(name string, fn any) (*BuiltinFunction, error) {
	argc, err := getFuncNumArgs(fn)
	if err != nil {
		return nil, err
	}

	return &BuiltinFunction{
		name: name,
		fn:   fn,
		argc: argc,
	}, nil
}

func getFuncNumArgs(fn any) (int, error) {
	typ := reflect.TypeOf(fn)
	if typ.Kind() != reflect.Func {
		return 0, ErrNotAFunction
	}
	return typ.NumIn(), nil
}

func BuiltinLen(a Object) (Object, error) {
	if a.IsLenable() {
		al := a.(lenable)
		return &Integer{Value: int64(al.Len())}, nil
	}
	return nil, fmt.Errorf("object %s is not lenable", a.String())
}

func BuiltinIsNull(v Object) (Object, error) {
	return &Bool{Value: v.IsNull()}, nil
}

func BuiltinFirst(a Object) (Object, error) {
	if a.IsArray() {
		arr := a.(*Array)
		if len(arr.items) > 0 {
			return arr.items[0], nil
		}
		return &Null{}, nil
	} else if a.IsString() {
		str := a.(*String)
		if len(str.Value) > 0 {
			return &String{Value: string(str.Value[0])}, nil
		}
		return &Null{}, nil
	}
	return nil, fmt.Errorf("object %s is not array or string", a.String())
}

func BuiltinLast(a Object) (Object, error) {
	if a.IsArray() {
		arr := a.(*Array)
		if len(arr.items) > 0 {
			return arr.items[len(arr.items)-1], nil
		}
		return &Null{}, nil
	} else if a.IsString() {
		str := a.(*String)
		if len(str.Value) > 0 {
			return &String{Value: string(str.Value[len(str.Value)-1])}, nil
		}
		return &Null{}, nil
	}
	return nil, fmt.Errorf("object %s is not array or string", a.String())
}

func BuiltinRest(a Object) (Object, error) {
	if a.IsArray() {
		arr := a.(*Array)
		if len(arr.items) > 1 {
			return &Array{items: arr.items[1:]}, nil
		}
		return &Null{}, nil
	} else if a.IsString() {
		str := a.(*String)
		if len(str.Value) > 1 {
			return &String{Value: str.Value[1:]}, nil
		}
		return &Null{}, nil
	}
	return nil, fmt.Errorf("object %s is not array or string", a.String())
}
