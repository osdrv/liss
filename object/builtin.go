package object

import (
	"fmt"
	"io"
	"math/rand/v2"
	"os"
	"osdrv/liss/regexp"
	"path/filepath"
	"strconv"
	"time"
)

var (
	ErrNotAFunction = fmt.Errorf("not a function")
)

type builtinFuncRaw func(args []Object) (Object, error)

type BuiltinFunc struct {
	defaultObject
	Name     string
	Argc     int
	Variadic bool
	Fn       builtinFuncRaw
}

func NewBuiltinFunc(name string, argc int, variadic bool, fn builtinFuncRaw) *BuiltinFunc {
	return &BuiltinFunc{
		Name:     name,
		Argc:     argc,
		Variadic: variadic,
		Fn:       fn,
	}
}

var _ Object = (*BuiltinFunc)(nil)

func (b *BuiltinFunc) Type() ObjectType {
	return BuiltinType
}

func (b *BuiltinFunc) Clone() Object {
	return b
}

func (b *BuiltinFunc) Raw() any {
	return b.Fn
}

func (b *BuiltinFunc) String() string {
	return "<builtin function " + b.Name + ">"
}

func (b *BuiltinFunc) validateArgs(args []Object) error {
	// TODO: function signature could be more complex in the future
	// We could do type checking here as well
	if !b.Variadic {
		if len(args) != b.Argc {
			return fmt.Errorf("builtin function %s expects %d arguments, got %d", b.Name, b.Argc, len(args))
		}
	} else {
		// Variadic function: only check minimum number of arguments
		if len(args) < b.Argc {
			return fmt.Errorf("builtin function %s expects at least %d arguments, got %d", b.Name, b.Argc, len(args))
		}
	}
	return nil
}

func (b *BuiltinFunc) IsFunction() bool {
	return true
}

func (b *BuiltinFunc) Invoke(args []Object) (Object, error) {
	if err := b.validateArgs(args); err != nil {
		return nil, err
	}
	return b.Fn(args)
}

var builtins []*BuiltinFunc
var builtinIndex map[string]int = make(map[string]int)

func init() {
	builtins = []*BuiltinFunc{
		NewBuiltinFunc("time", 0, false, builtinTime),
		NewBuiltinFunc("time_ms", 0, false, builtinTimeMillis),
		NewBuiltinFunc("rand", 0, false, builtinRandInt),
		NewBuiltinFunc("randn", 1, false, builtinRandIntN),

		NewBuiltinFunc("len", 1, false, builtinLen),
		NewBuiltinFunc("head", 1, false, builtinHead),
		NewBuiltinFunc("last", 1, false, builtinLast),
		NewBuiltinFunc("tail", 1, false, builtinTail),
		NewBuiltinFunc("str", 1, false, builtinStr),
		NewBuiltinFunc("int", 1, false, builtinInt),

		NewBuiltinFunc("list", 0, true, builtinList),
		NewBuiltinFunc("range", 3, false, builtinRange),
		NewBuiltinFunc("dict", 0, true, builtinDict),
		NewBuiltinFunc("get", 2, false, builtinGet),
		NewBuiltinFunc("put", 3, false, builtinPut),
		NewBuiltinFunc("del", 2, false, builtinDel),
		NewBuiltinFunc("has?", 2, false, builtinHas),
		NewBuiltinFunc("keys", 1, false, builtinKeys),
		NewBuiltinFunc("values", 1, false, builtinValues),
		NewBuiltinFunc("re", 1, false, builtinReCompile),
		NewBuiltinFunc("match?", 2, false, builtinReMatch),
		NewBuiltinFunc("match", 2, false, builtinReCapture),
		NewBuiltinFunc("match_ix", 2, false, builtinReCaptureIx),
		NewBuiltinFunc("print", 2, true, builtinPrint),
		NewBuiltinFunc("println", 2, true, builtinPrintln),

		NewBuiltinFunc("fopen", 1, true, builtinFOpen),
		NewBuiltinFunc("fclose", 1, false, builtinFClose),
		NewBuiltinFunc("fread_all", 1, false, builtinFReadAll),

		NewBuiltinFunc("is_empty?", 1, false, builtinEmpty),
		NewBuiltinFunc("is_null?", 1, false, builtinIsNull),
		NewBuiltinFunc("is_list?", 1, false, builtinIsList),
		NewBuiltinFunc("is_dict?", 1, false, builtinIsDictionary),
		NewBuiltinFunc("is_string?", 1, false, builtinIsString),
		NewBuiltinFunc("is_int?", 1, false, builtinIsInt),
		NewBuiltinFunc("is_float?", 1, false, builtinIsFloat),
		NewBuiltinFunc("is_bool?", 1, false, builtinIsBool),

		// TODO: this is cheating. Go implement your own parsing primitives, Oleg!
		NewBuiltinFunc("parse_int", 1, false, builtinParseInt),
		NewBuiltinFunc("parse_float", 1, false, builtinParseFloat),
	}

	for ix, b := range builtins {
		builtinIndex[b.Name] = ix
	}
}

func GetBuiltinByName(name string) (*BuiltinFunc, int, bool) {
	if ix, ok := builtinIndex[name]; ok {
		return builtins[ix], ix, true
	}
	return nil, -1, false
}

func GetBuiltinByIndex(ix int) (*BuiltinFunc, bool) {
	if ix < 0 || ix >= len(builtins) {
		return nil, false
	}
	return builtins[ix], true
}

func builtinTime(_ []Object) (Object, error) {
	return NewInteger(int64(time.Now().Unix())), nil
}

func builtinTimeMillis(_ []Object) (Object, error) {
	return NewInteger(int64(time.Now().UnixMilli())), nil
}

func builtinRandInt(_ []Object) (Object, error) {
	n := rand.Int64()
	return NewInteger(n), nil
}

func builtinRandIntN(args []Object) (Object, error) {
	n := args[0]
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

func builtinLen(args []Object) (Object, error) {
	a := args[0]
	if a.IsLenable() {
		al := a.(lenable)
		return &Integer{Value: int64(al.Len())}, nil
	}
	return nil, fmt.Errorf("object %s is not lenable", a.String())
}

func builtinEmpty(args []Object) (Object, error) {
	a := args[0]
	if a.IsLenable() {
		al := a.(lenable)
		return &Bool{Value: al.Len() == 0}, nil
	}
	return nil, fmt.Errorf("object %s is not lennable", a.String())
}

func builtinIsNull(args []Object) (Object, error) {
	v := args[0]
	return &Bool{Value: v.IsNull()}, nil
}

func builtinStr(args []Object) (Object, error) {
	a := args[0]
	return &String{Value: []rune(a.String())}, nil
}

func builtinInt(args []Object) (Object, error) {
	a := args[0]
	switch a.Type() {
	case IntegerType:
		return a, nil
	case FloatType:
		v := a.(*Float).Value
		return NewInteger(int64(v)), nil
	default:
		return nil, fmt.Errorf("cannot convert %s to int", a.String())
	}
}

func builtinHead(args []Object) (Object, error) {
	a := args[0]
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

func builtinLast(args []Object) (Object, error) {
	a := args[0]
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

func builtinTail(args []Object) (Object, error) {
	a := args[0]
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

func builtinList(args []Object) (Object, error) {
	return &List{items: args}, nil
}

func builtinRange(args []Object) (Object, error) {
	arrObj, fromObj, toObj := args[0], args[1], args[2]
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

func builtinDict(pairs []Object) (Object, error) {
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

func builtinGetFromDict(args []Object) (Object, error) {
	container, key := args[0], args[1]
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

func builtinGetFromList(args []Object) (Object, error) {
	container, key := args[0], args[1]
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

func builtinGetFromString(args []Object) (Object, error) {
	container, key := args[0], args[1]
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

func builtinGet(args []Object) (Object, error) {
	container := args[0]
	if container.IsList() {
		return builtinGetFromList(args)
	} else if container.IsDictionary() {
		return builtinGetFromDict(args)
	} else if container.IsString() {
		return builtinGetFromString(args)
	}
	return nil, fmt.Errorf("get: expected list or dictionary as first argument, got %s", container.String())
}

func builtinPut(args []Object) (Object, error) {
	container, key, value := args[0], args[1], args[2]
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

func builtinDel(args []Object) (Object, error) {
	container, key := args[0], args[1]
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

func builtinHas(args []Object) (Object, error) {
	container, key := args[0], args[1]
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

func builtinKeys(args []Object) (Object, error) {
	container := args[0]
	if !container.IsDictionary() {
		return nil, fmt.Errorf("keys: expected dictionary as argument, got %s", container.String())
	}
	dict := container.(*Dictionary)
	return NewList(dict.Keys()), nil
}

func builtinValues(args []Object) (Object, error) {
	container := args[0]
	if !container.IsDictionary() {
		return nil, fmt.Errorf("values: expected dictionary as argument, got %s", container.String())
	}
	dict := container.(*Dictionary)
	return NewList(dict.Values()), nil
}

func builtinReCompile(args []Object) (Object, error) {
	pat := args[0]
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

func builtinReMatch(args []Object) (Object, error) {
	pat, str := args[0], args[1]
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
		if re.Match(sstr) {
			return TRUE, nil
		}
		return FALSE, nil
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

func prepareMatchArgs(args []Object) (*regexp.Regexp, string, error) {
	pat, str := args[0], args[1]
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

func builtinReCapture(args []Object) (Object, error) {
	re, sstr, err := prepareMatchArgs(args)
	if err != nil {
		return nil, err
	}
	return finalizerMatchString(re, sstr)
}

func builtinReCaptureIx(args []Object) (Object, error) {
	re, sstr, err := prepareMatchArgs(args)
	if err != nil {
		return nil, err
	}
	return finalizerMatchStringIx(re, sstr)
}

func builtinPrint(args []Object) (Object, error) {
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

func builtinPrintln(args []Object) (Object, error) {
	args = append(args, NewString("\n"))
	return builtinPrint(args)
}

// TODO: add modes (read, write, append, etc.)
func builtinFOpen(args []Object) (Object, error) {
	pathObj := args[0]
	if !pathObj.IsString() {
		return nil, fmt.Errorf("fopen: expected string as argument, got %s", pathObj.String())
	}
	var dotPath Object
	if len(args) > 1 {
		dotPath = args[1]
		if !dotPath.IsString() {
			return nil, fmt.Errorf("fopen: expected string as second argument, got %s", dotPath.String())
		}
	}
	path := string(pathObj.(*String).Value)
	if filepath.IsLocal(path) {
		if dotPath != nil {
			basePath := string(dotPath.(*String).Value)
			path = filepath.Join(basePath, path)
		}
	}
	// open file for reading
	fd, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	return &File{fd: fd}, nil
}

func builtinFClose(args []Object) (Object, error) {
	f := args[0]
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

func builtinFReadAll(args []Object) (Object, error) {
	f := args[0]
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

func builtinIsList(args []Object) (Object, error) {
	v := args[0]
	return &Bool{Value: v.IsList()}, nil
}

func builtinIsDictionary(args []Object) (Object, error) {
	v := args[0]
	return &Bool{Value: v.IsDictionary()}, nil
}

func builtinIsString(args []Object) (Object, error) {
	v := args[0]
	return &Bool{Value: v.IsString()}, nil
}

func builtinIsInt(args []Object) (Object, error) {
	v := args[0]
	_, ok := v.(*Integer)
	return &Bool{Value: ok}, nil
}

func builtinIsFloat(args []Object) (Object, error) {
	v := args[0]
	_, ok := v.(*Float)
	return &Bool{Value: ok}, nil
}

func builtinIsBool(args []Object) (Object, error) {
	v := args[0]
	return &Bool{Value: v.IsBool()}, nil
}

func builtinParseInt(args []Object) (Object, error) {
	v := args[0]
	if !v.IsString() {
		return nil, fmt.Errorf("parse_int: expected string as argument, got %s", v.String())
	}
	str := string(v.(*String).Value)
	intv, err := strconv.ParseInt(str, 10, 64)
	if err != nil {
		return nil, err
	}
	return NewInteger(intv), nil
}

func builtinParseFloat(args []Object) (Object, error) {
	v := args[0]
	if !v.IsString() {
		return nil, fmt.Errorf("parse_float: expected string as argument, got %s", v.String())
	}
	str := string(v.(*String).Value)
	floatv, err := strconv.ParseFloat(str, 64)
	if err != nil {
		return nil, err
	}
	return NewFloat(floatv), nil
}
