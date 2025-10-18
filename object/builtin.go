package object

import (
	"fmt"
	"reflect"
)

var (
	ErrNotAFunction = fmt.Errorf("not a function")
)

type indexedBuiltin struct {
	ix int
	fn *BuiltinFunction
}

func mkIndexedBuiltin(ix int, name string, fn any) *indexedBuiltin {
	bfn, err := NewBuiltinFunction(name, fn)
	if err != nil {
		panic(fmt.Sprintf("failed to create builtin function %s: %s", name, err))
	}
	return &indexedBuiltin{ix: ix, fn: bfn}
}

var builtinList []*indexedBuiltin
var builtinIndex map[string]*indexedBuiltin = make(map[string]*indexedBuiltin)

func init() {
	builtinList = []*indexedBuiltin{
		mkIndexedBuiltin(0, "len", builtinLen),
		mkIndexedBuiltin(1, "first", builtinFirst),
		mkIndexedBuiltin(2, "last", builtinLast),
		mkIndexedBuiltin(3, "rest", builtinRest),
		mkIndexedBuiltin(4, "isNull", builtinIsNull),
	}

	for _, b := range builtinList {
		builtinIndex[b.fn.name] = b
	}
}

func GetBuiltinByName(name string) (*BuiltinFunction, int, bool) {
	if b, ok := builtinIndex[name]; ok {
		return b.fn, b.ix, true
	}
	return nil, -1, false
}

func GetBuiltinByIndex(ix int) (*BuiltinFunction, bool) {
	if ix < 0 || ix >= len(builtinList) {
		return nil, false
	}
	return builtinList[ix].fn, true
}

type BuiltinFunction struct {
	defaultObject
	name  string
	fn    any // TODO: reevaluate whether I need to keep the src fn
	argc  int
	rawfn reflect.Value
}

var _ Object = (*BuiltinFunction)(nil)

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

func (b *BuiltinFunction) Invoke(args ...Object) (Object, error) {
	if len(args) != b.argc {
		return nil, fmt.Errorf("builtin function %s expects %d arguments, got %d", b.name, b.argc, len(args))
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

func builtinFirst(a Object) (Object, error) {
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

func builtinLast(a Object) (Object, error) {
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

func builtinRest(a Object) (Object, error) {
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
