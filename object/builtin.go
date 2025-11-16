package object

import (
	"fmt"
	"io"
	"math/rand/v2"
	"os"
	"osdrv/liss/regexp"
	"reflect"
	"time"
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
		mkBuiltin("time", builtinTime, false),
		mkBuiltin("time_ms", builtinTimeMillis, false),
		mkBuiltin("rand", builtinRandInt, false),
		mkBuiltin("randn", builtinRandIntN, false),

		mkBuiltin("len", builtinLen, false),
		mkBuiltin("head", builtinHead, false),
		mkBuiltin("last", builtinLast, false),
		mkBuiltin("tail", builtinTail, false),
		mkBuiltin("str", builtinStr, false),

		mkBuiltin("list", builtinList, true),
		mkBuiltin("range", builtinRange, false),
		mkBuiltin("dict", builtinDict, true),
		mkBuiltin("get", builtinGet, false),
		mkBuiltin("put", builtinPut, false),
		mkBuiltin("del", builtinDel, false),
		mkBuiltin("has", builtinHas, false),
		mkBuiltin("keys", builtinKeys, false),
		mkBuiltin("values", builtinValues, false),
		mkBuiltin("re", builtinReCompile, false),
		mkBuiltin("match?", builtinReMatch, false),
		mkBuiltin("match", builtinReCapture, false),
		mkBuiltin("match_ix", builtinReCaptureIx, false),
		mkBuiltin("print", builtinPrint, true),
		mkBuiltin("println", builtinPrintln, true),

		mkBuiltin("fopen", builtinFOpen, false),
		mkBuiltin("fclose", builtinFClose, false),
		mkBuiltin("fread_all", builtinFReadAll, false),

		mkBuiltin("is_empty?", builtinEmpty, false),
		mkBuiltin("is_null?", builtinIsNull, false),
		mkBuiltin("is_list?", builtinIsList, false),
		mkBuiltin("is_dict?", builtinIsDictionary, false),
		mkBuiltin("is_string?", builtinIsString, false),
		mkBuiltin("is_integer?", builtinIsInteger, false),
		mkBuiltin("is_float?", builtinIsFloat, false),
		mkBuiltin("is_bool?", builtinIsBool, false),
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

func (b *BuiltinFunction) Name() string {
	return b.name
}

func (b *BuiltinFunction) Clone() Object {
	return b
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

	var err error
	var res Object
	if out[1].Interface() != nil {
		err = out[1].Interface().(error)
	} else {
		res = out[0].Interface().(Object)
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

func builtinTime() (Object, error) {
	return NewInteger(int64(time.Now().Unix())), nil
}

func builtinTimeMillis() (Object, error) {
	return NewInteger(int64(time.Now().UnixMilli())), nil
}

func builtinRandInt() (Object, error) {
	n := rand.Int64()
	return NewInteger(n), nil
}

func builtinRandIntN(n Object) (Object, error) {
	if n.Type() != IntegerType {
		return nil, fmt.Errorf("rand:intn: expected integer argument, got %s", n.String())
	}
	nInt := n.(*Integer).Value
	if nInt <= 0 {
		return nil, fmt.Errorf("rand:intn: argument must be positive, got %d", nInt)
	}
	r := rand.Int64N(nInt)
	return NewInteger(int64(r)), nil
}

func builtinLen(a Object) (Object, error) {
	if a.IsLenable() {
		al := a.(lenable)
		return &Integer{Value: int64(al.Len())}, nil
	}
	return nil, fmt.Errorf("object %s is not lenable", a.String())
}

func builtinEmpty(a Object) (Object, error) {
	if a.IsLenable() {
		al := a.(lenable)
		return &Bool{Value: al.Len() == 0}, nil
	}
	return nil, fmt.Errorf("object %s is not lennable", a.String())
}

func builtinIsNull(v Object) (Object, error) {
	return &Bool{Value: v.IsNull()}, nil
}

func builtinStr(a Object) (Object, error) {
	return &String{Value: []rune(a.String())}, nil
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
			return &String{Value: (str.Value[0:1])}, nil
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
			return &String{Value: []rune{str.Value[len(str.Value)-1]}}, nil
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
		// empty list
		return &List{}, nil
	} else if a.IsString() {
		str := a.(*String)
		if len(str.Value) > 1 {
			return &String{Value: str.Value[1:]}, nil
		}
		return &String{}, nil
	}
	return nil, fmt.Errorf("object %s is not array or string", a.String())
}

func builtinList(args ...Object) (Object, error) {
	return &List{items: args}, nil
}

func builtinRange(arrObj, fromObj, toObj Object) (Object, error) {
	if _, ok := fromObj.(*Integer); !ok {
		return nil, fmt.Errorf("range: expected integer as from argument, got %s", fromObj.String())
	}
	if _, ok := toObj.(*Integer); !ok {
		return nil, fmt.Errorf("range: expected integer as to argument, got %s", toObj.String())
	}
	from := fromObj.(*Integer).Value
	to := toObj.(*Integer).Value

	switch arr := arrObj.(type) {
	case *List:
		if from < 0 || from > to {
			return nil, fmt.Errorf("range: invalid range [%d:%d] for list of length %d", from, to, len(arr.items))
		}
		to = min(to, int64(len(arr.items)))
		cp := make([]Object, to-from)
		copy(cp, arr.items[from:to])
		return &List{items: cp}, nil
	case *String:
		if from < 0 || from > to {
			return nil, fmt.Errorf("range: invalid range [%d:%d] for string of length %d", from, to, len(arr.Value))
		}
		to = min(to, int64(len(arr.Value)))
		cp := make([]rune, to-from)
		copy(cp, arr.Value[from:to])
		return &String{Value: cp}, nil
	default:
		return nil, fmt.Errorf("range: expected list or string as first argument, got %s", arrObj.String())
	}
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

func builtinGetFromDict(container Object, key Object) (Object, error) {
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

func builtinGetFromList(container Object, key Object) (Object, error) {
	if !container.IsList() {
		return nil, fmt.Errorf("get: expected list as first argument, got %s", container.String())
	}
	list := container.(*List)
	indexObj, ok := key.(*Integer)
	if !ok {
		return nil, fmt.Errorf("get: expected integer as index for list, got %s", key.String())
	}
	index := indexObj.Value
	if index < 0 || int(index) >= len(list.items) {
		return &Null{}, nil
	}
	return list.items[index], nil
}

func builtinGetFromString(container Object, key Object) (Object, error) {
	if !container.IsString() {
		return nil, fmt.Errorf("get: expected string as first argument, got %s", container.String())
	}
	str := container.(*String)
	indexObj, ok := key.(*Integer)
	if !ok {
		return nil, fmt.Errorf("get: expected integer as index for string, got %s", key.String())
	}
	index := indexObj.Value
	if index < 0 || int(index) >= len(str.Value) {
		return &Null{}, nil
	}
	return &String{Value: []rune{str.Value[index]}}, nil
}

func builtinGet(container Object, key Object) (Object, error) {
	if container.IsList() {
		return builtinGetFromList(container, key)
	} else if container.IsDictionary() {
		return builtinGetFromDict(container, key)
	} else if container.IsString() {
		return builtinGetFromString(container, key)
	}
	return nil, fmt.Errorf("get: expected list or dictionary as first argument, got %s", container.String())
}

func builtinPut(container Object, key Object, value Object) (Object, error) {
	if container.IsList() {
		if key.Type() != IntegerType {
			return nil, fmt.Errorf("put: expected integer as index for list, got %s", key.String())
		}
		list := container.(*List)
		ix := key.(*Integer).Value
		if ix < 0 || int(ix) >= len(list.items) {
			return nil, fmt.Errorf("put: index %d out of bounds for list of length %d", ix, len(list.items))
		}
		list.items[ix] = value
		return &Null{}, nil
	} else if container.IsDictionary() {
		dict := container.(*Dictionary)
		err := dict.Put(key, value)
		if err != nil {
			return nil, err
		}
		return &Null{}, nil
	} else {
		return nil, fmt.Errorf("put: expected list or dictionary as first argument, got %s", container.String())
	}
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

func builtinHas(container Object, key Object) (Object, error) {
	if !container.IsDictionary() {
		return nil, fmt.Errorf("has: expected dictionary as first argument, got %s", container.String())
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

func builtinReCompile(pat Object) (Object, error) {
	if !pat.IsString() {
		return nil, fmt.Errorf("compile: expected string as argument, got %s", pat.String())
	}
	strPat := pat.(*String)
	re, err := NewRegexp(string(strPat.Value))
	if err != nil {
		return nil, err
	}
	return re, nil
}

func builtinReMatch(pat Object, str Object) (Object, error) {
	if !str.IsString() {
		return nil, fmt.Errorf("match: expected string as second argument, got %s", str.String())
	}
	sstr := string(str.(*String).Value)
	// pat could be a Regexp object or a string
	if pat.IsRegexp() {
		re := pat.(*Regexp)
		return &Bool{Value: re.Match(sstr)}, nil
	} else if pat.IsString() {
		strPat := pat.(*String)
		re, err := NewRegexp(string(strPat.Value))
		if err != nil {
			return nil, err
		}
		return NewBool(re.Match(sstr)), nil
	}
	return nil, fmt.Errorf("match: expected regexp or string as first argument, got %s", pat.String())
}

func finalizerMatchString(re *regexp.Regexp, str string) (Object, error) {
	capts, ok := re.MatchString(str)
	res := &List{items: []Object{}}
	if ok {
		for _, match := range capts {
			res.items = append(res.items, NewString(match))
		}
	}
	return res, nil
}

func finalizerMatchStringIx(re *regexp.Regexp, str string) (Object, error) {
	ixs, ok := re.MatchStringIx(str)
	items := make([]Object, 0, len(ixs))
	res := &List{}
	if ok {
		for _, pair := range ixs {
			sublist := &List{items: []Object{
				NewInteger(int64(pair[0])),
				NewInteger(int64(pair[1])),
			}}
			items = append(items, sublist)
		}
	}
	res.items = items
	return res, nil
}

func prepareMatchArgs(pat Object, str Object) (*regexp.Regexp, string, error) {
	if !str.IsString() {
		return nil, "", fmt.Errorf("capture: expected string as second argument, got %s", str.String())
	}
	sstr := string(str.(*String).Value)
	// pat could be a Regexp object or a string
	var re *regexp.Regexp
	if pat.IsRegexp() {
		re = pat.(*Regexp).re
	} else if pat.IsString() {
		strPat := pat.(*String)
		var err error
		re, err = regexp.Compile(string(strPat.Value))
		if err != nil {
			return nil, "", err
		}
	} else {
		return nil, "", fmt.Errorf("capture: expected regexp or string as first argument, got %s", pat.String())
	}
	return re, sstr, nil
}

func builtinReCapture(pat Object, str Object) (Object, error) {
	re, sstr, err := prepareMatchArgs(pat, str)
	if err != nil {
		return nil, err
	}
	return finalizerMatchString(re, sstr)
}

func builtinReCaptureIx(pat Object, str Object) (Object, error) {
	re, sstr, err := prepareMatchArgs(pat, str)
	if err != nil {
		return nil, err
	}
	return finalizerMatchStringIx(re, sstr)
}

func builtinPrint(args ...Object) (Object, error) {
	if len(args) < 2 {
		return nil, fmt.Errorf("print: expected at least 2 arguments, got %d", len(args))
	}
	f := args[0]
	rest := args[1:]
	if !f.IsFile() {
		return nil, fmt.Errorf("print: expected file as first argument, got %s", f.String())
	}
	file := f.(*File)
	var str string
	totBytes := 0
	for _, obj := range rest {
		switch obj.(type) {
		case *String:
			str = string(obj.(*String).Value)
		default:
			str = obj.String()
		}

		if n, err := file.fd.WriteString(str); err != nil {
			return NewInteger(0), err
		} else {
			totBytes += n
		}
	}
	return NewInteger(int64(totBytes)), nil
}

func builtinPrintln(args ...Object) (Object, error) {
	args = append(args, NewString("\n"))
	return builtinPrint(args...)
}

// TODO: add modes (read, write, append, etc.)
func builtinFOpen(path Object) (Object, error) {
	if !path.IsString() {
		return nil, fmt.Errorf("fopen: expected string as argument, got %s", path.String())
	}
	strPath := path.(*String)
	// open file for reading
	fd, err := os.Open(string(strPath.Value))
	if err != nil {
		return nil, err
	}
	return &File{fd: fd}, nil
}

func builtinFClose(f Object) (Object, error) {
	if !f.IsFile() {
		return nil, fmt.Errorf("fclose: expected file as argument, got %s", f.String())
	}
	file := f.(*File)
	err := file.fd.Close()
	if err != nil {
		return nil, err
	}
	return &Null{}, nil
}

func builtinFReadAll(f Object) (Object, error) {
	if !f.IsFile() {
		return nil, fmt.Errorf("fread_all: expected file as argument, got %s", f.String())
	}
	file := f.(*File)
	data, err := io.ReadAll(file.fd)
	if err != nil {
		return nil, err
	}
	// TODO: implement blobs
	return &String{Value: []rune(string(data))}, nil
}

func builtinIsList(v Object) (Object, error) {
	return &Bool{Value: v.IsList()}, nil
}

func builtinIsDictionary(v Object) (Object, error) {
	return &Bool{Value: v.IsDictionary()}, nil
}

func builtinIsString(v Object) (Object, error) {
	return &Bool{Value: v.IsString()}, nil
}

func builtinIsInteger(v Object) (Object, error) {
	_, ok := v.(*Integer)
	return &Bool{Value: ok}, nil
}

func builtinIsFloat(v Object) (Object, error) {
	_, ok := v.(*Float)
	return &Bool{Value: ok}, nil
}

func builtinIsBool(v Object) (Object, error) {
	return &Bool{Value: v.IsBool()}, nil
}
