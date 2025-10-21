package object

import (
	"fmt"
	"reflect"
)

var (
	ErrNotAFunction = fmt.Errorf("not a function")
)

var builtins []*BuiltinFunction
var builtinIndex map[string]int = make(map[string]int)

func mkBuiltin(name string, fn any, variadic bool) *BuiltinFunction {
	b, err := NewBuiltinFunction(name, fn)
	if err != nil {
		panic(err)
	}
	b.variadic = variadic
	return b
}

func init() {
	builtins = []*BuiltinFunction{
		mkBuiltin("len", builtinLen, false),
		mkBuiltin("head", builtinHead, false),
		mkBuiltin("last", builtinLast, false),
		mkBuiltin("tail", builtinTail, false),
		mkBuiltin("isNull", builtinIsNull, false),

		mkBuiltin("list", builtinList, true),
		// mkBuiltin("find", builtinFind, false),
		// mkBuiltin("findAll", builtinFindAll, false),
		// mkBuiltin("range", builtinRange, false),
		mkBuiltin("dict", builtinDict, true),
		mkBuiltin("get", builtinGet, false),
		mkBuiltin("put", builtinPut, false),
		mkBuiltin("del", builtinDel, false),
		mkBuiltin("isset", builtinIsSet, false),
		mkBuiltin("keys", builtinKeys, false),
		mkBuiltin("values", builtinValues, false),
	}

	for ix, b := range builtins {
		builtinIndex[b.name] = ix
	}
}

func GetBuiltinByName(name string) (*BuiltinFunction, int, bool) {
	if ix, ok := builtinIndex[name]; ok {
		return builtins[ix], ix, true
	}
	return nil, -1, false
}

func GetBuiltinByIndex(ix int) (*BuiltinFunction, bool) {
	if ix < 0 || ix >= len(builtins) {
		return nil, false
	}
	return builtins[ix], true
}

type BuiltinFunction struct {
	defaultObject
	name     string
	fn       any // TODO: reevaluate whether I need to keep the src fn
	variadic bool
	argc     int
	rawfn    reflect.Value
}

var _ Object = (*BuiltinFunction)(nil)

func NewVariadicBuiltinFunction(name string, fn any) (*BuiltinFunction, error) {
	b, err := NewBuiltinFunction(name, fn)
	if err != nil {
		return nil, err
	}
	b.variadic = true
	return b, nil
}

func NewBuiltinFunction(name string, fn any) (*BuiltinFunction, error) {
	argc, err := getFuncNumArgs(fn)
	if err != nil {
		return nil, err
	}

	rawfn := reflect.ValueOf(fn)
	if rawfn.Kind() != reflect.Func {
		return nil, ErrNotAFunction
	}

	return &BuiltinFunction{
		name:  name,
		fn:    fn,
		argc:  argc,
		rawfn: rawfn,
	}, nil
}

func (b *BuiltinFunction) String() string {
	return fmt.Sprintf("<builtin function %s>", b.name)
}

func (b *BuiltinFunction) Type() ObjectType {
	return BuiltinType
}

func (b *BuiltinFunction) IsFunction() bool {
	return true
}

func (b *BuiltinFunction) validateArgs(args []Object) error {
	if b.variadic {
		if len(args) < b.argc-1 {
			return fmt.Errorf("builtin function %s expects at least %d arguments, got %d", b.name, b.argc-1, len(args))
		}
	} else {
		if len(args) != b.argc {
			return fmt.Errorf("builtin function %s expects %d arguments, got %d", b.name, b.argc, len(args))
		}
	}
	return nil
}

func (b *BuiltinFunction) Invoke(args ...Object) (Object, error) {
	if err := b.validateArgs(args); err != nil {
		return nil, err
	}

	in := make([]reflect.Value, len(args))
	for i, arg := range args {
		in[i] = reflect.ValueOf(arg)
	}

	out := b.rawfn.Call(in)
	if len(out) != 2 {
		return nil, fmt.Errorf("builtin function %s should return two values (Object, error)", b.name)
	}

	res := out[0].Interface().(Object)
	var err error
	if out[1].Interface() != nil {
		err = out[1].Interface().(error)
	}

	return res, err
}

func (b *BuiltinFunction) Raw() any {
	panic("not implemented")
}

func getFuncNumArgs(fn any) (int, error) {
	typ := reflect.TypeOf(fn)
	if typ.Kind() != reflect.Func {
		return 0, ErrNotAFunction
	}
	return typ.NumIn(), nil
}

func builtinLen(a Object) (Object, error) {
	if a.IsLenable() {
		al := a.(lenable)
		return &Integer{Value: int64(al.Len())}, nil
	}
	return nil, fmt.Errorf("object %s is not lenable", a.String())
}

func builtinIsNull(v Object) (Object, error) {
	return &Bool{Value: v.IsNull()}, nil
}

func builtinHead(a Object) (Object, error) {
	if a.IsList() {
		arr := a.(*List)
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

func builtinLast(a Object) (Object, error) {
	if a.IsList() {
		arr := a.(*List)
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

func builtinTail(a Object) (Object, error) {
	if a.IsList() {
		arr := a.(*List)
		if len(arr.items) > 1 {
			return &List{items: arr.items[1:]}, nil
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

func builtinList(args ...Object) (Object, error) {
	return &List{items: args}, nil
}

func builtinDict(pairs ...Object) (Object, error) {
	dict := make(map[any]Object)
	for _, pair := range pairs {
		if !pair.IsList() {
			return nil, fmt.Errorf("dict expects list pairs, got %s", pair.String())
		}
		list := pair.(*List)
		if list.Len() != 2 {
			return nil, fmt.Errorf("dict expects list pairs of length 2, got %d", list.Len())
		}
		dict[list.items[0].Raw()] = list.items[1]
	}
	return NewDictionaryWithItems(pairs)
}

func builtinGet(container Object, key Object) (Object, error) {
	if !container.IsDictionary() {
		return nil, fmt.Errorf("get: expected dictionary as first argument, got %s", container.String())
	}
	dict := container.(*Dictionary)
	value, ok, err := dict.Get(key)
	if err != nil {
		return nil, err
	}
	if !ok {
		return &Null{}, nil
	}
	return value, nil
}

func builtinPut(container Object, key Object, value Object) (Object, error) {
	if !container.IsDictionary() {
		return nil, fmt.Errorf("put: expected dictionary as first argument, got %s", container.String())
	}
	dict := container.(*Dictionary)
	err := dict.Put(key, value)
	if err != nil {
		return nil, err
	}
	return &Null{}, nil
}

func builtinDel(container Object, key Object) (Object, error) {
	if !container.IsDictionary() {
		return nil, fmt.Errorf("del: expected dictionary as first argument, got %s", container.String())
	}
	dict := container.(*Dictionary)
	ok, err := dict.Delete(key)
	if err != nil {
		return nil, err
	}
	return &Bool{Value: ok}, nil
}

func builtinIsSet(container Object, key Object) (Object, error) {
	if !container.IsDictionary() {
		return nil, fmt.Errorf("isset: expected dictionary as first argument, got %s", container.String())
	}
	dict := container.(*Dictionary)
	_, ok, err := dict.Get(key)
	if err != nil {
		return nil, err
	}
	return &Bool{Value: ok}, nil
}

func builtinKeys(container Object) (Object, error) {
	if !container.IsDictionary() {
		return nil, fmt.Errorf("keys: expected dictionary as argument, got %s", container.String())
	}
	dict := container.(*Dictionary)
	return NewList(dict.Keys()), nil
}

func builtinValues(container Object) (Object, error) {
	if !container.IsDictionary() {
		return nil, fmt.Errorf("values: expected dictionary as argument, got %s", container.String())
	}
	dict := container.(*Dictionary)
	return NewList(dict.Values()), nil
}
