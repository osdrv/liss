package object

import (
	"bytes"
	"osdrv/liss/code"
	"osdrv/liss/regexp"
	"strconv"
)

type ObjectType uint8

const (
	_ ObjectType = iota
	IntegerType
	FloatType
	BoolType
	StringType
	NullType
	FunctionType
	BuiltinType
	ClosureType
	ListType
	DictionaryType
	RegexpType
	FileType
	ModuleType
)

var (
	TRUE  = NewBool(true)
	FALSE = NewBool(false)
	NULL  = NewNull()
)

func (t ObjectType) String() string {
	switch t {
	case IntegerType:
		return "Integer"
	case FloatType:
		return "Float"
	case BoolType:
		return "Bool"
	case StringType:
		return "String"
	case NullType:
		return "Null"
	case FunctionType:
		return "Function"
	case BuiltinType:
		return "Builtin"
	case ClosureType:
		return "Closure"
	case ListType:
		return "List"
	case DictionaryType:
		return "Dictionary"
	case RegexpType:
		return "Regexp"
	case FileType:
		return "File"
	default:
		return "Unknown"
	}
}

type Object interface {
	Type() ObjectType
	String() string
	IsNumeric() bool
	IsBool() bool
	IsString() bool
	IsNull() bool
	IsList() bool
	IsDictionary() bool
	IsFunction() bool
	IsLenable() bool
	IsRegexp() bool
	IsFile() bool
	IsModule() bool
	Raw() any
	Clone() Object
}

type defaultObject struct{}

func (d defaultObject) IsNumeric() bool {
	return false
}

func (d defaultObject) IsBool() bool {
	return false
}

func (d defaultObject) IsNull() bool {
	return false
}

func (d defaultObject) IsString() bool {
	return false
}

func (d defaultObject) IsList() bool {
	return false
}

func (d defaultObject) IsDictionary() bool {
	return false
}

func (d defaultObject) IsFunction() bool {
	return false
}

func (d defaultObject) IsLenable() bool {
	return false
}

func (d defaultObject) IsRegexp() bool {
	return false
}

func (d defaultObject) IsFile() bool {
	return false
}

func (d defaultObject) IsModule() bool {
	return false
}

type lenable interface {
	Len() int
}

type Numeric interface {
	Int64() int64
	Float64() float64
	ToFloat64() Object
}

type Integer struct {
	defaultObject
	Value int64
}

var _ Object = (*Integer)(nil)
var _ Numeric = (*Integer)(nil)

func NewInteger(value int64) *Integer {
	return &Integer{Value: value}
}

func (i *Integer) Clone() Object {
	return &Integer{Value: i.Value}
}

func (i *Integer) Type() ObjectType {
	return IntegerType
}

func (i *Integer) String() string {
	return strconv.FormatInt(i.Value, 10)
}

func (i *Integer) IsNumeric() bool {
	return true
}

func (i *Integer) Int64() int64 {
	return i.Value
}

func (i *Integer) Float64() float64 {
	return float64(i.Value)
}

func (i *Integer) ToFloat64() Object {
	return NewFloat(float64(i.Value))
}

func (i *Integer) Raw() any {
	return i.Value
}

type Float struct {
	defaultObject
	Value float64
}

var _ Object = (*Float)(nil)
var _ Numeric = (*Float)(nil)

func NewFloat(value float64) *Float {
	return &Float{Value: value}
}

func (f *Float) Clone() Object {
	return &Float{Value: f.Value}
}

func (f *Float) Type() ObjectType {
	return FloatType
}

func (f *Float) String() string {
	return strconv.FormatFloat(f.Value, 'f', -1, 64)
}

func (f *Float) Int64() int64 {
	return int64(f.Value)
}

func (f *Float) Float64() float64 {
	return f.Value
}

func (f *Float) ToFloat64() Object {
	return f
}

func (f *Float) IsNumeric() bool {
	return true
}

func (f *Float) Raw() any {
	return f.Value
}

type Bool struct {
	defaultObject
	Value bool
}

var _ Object = (*Bool)(nil)

func NewBool(value bool) *Bool {
	return &Bool{Value: value}
}

func (b *Bool) Clone() Object {
	return &Bool{Value: b.Value}
}

func (b *Bool) Type() ObjectType {
	return BoolType
}

func (b *Bool) String() string {
	return strconv.FormatBool(b.Value)
}

func (b *Bool) IsBool() bool {
	return true
}

func (b *Bool) Raw() any {
	return b.Value
}

type String struct {
	defaultObject
	Value []rune
}

var _ Object = (*String)(nil)
var _ lenable = (*String)(nil)

func NewString(value string) *String {
	return &String{Value: []rune(value)}
}

func (s *String) Clone() Object {
	return &String{Value: s.Value}
}

func (s *String) Type() ObjectType {
	return StringType
}

func (s *String) String() string {
	b := make([]rune, len(s.Value)+2)
	b[0] = '"'
	b[len(b)-1] = '"'
	copy(b[1:], s.Value)
	return string(b)
}

func (s *String) IsLenable() bool {
	return true
}

func (s *String) Len() int {
	return len(s.Value)
}

func (s *String) IsString() bool {
	return true
}

func (s *String) Raw() any {
	return string(s.Value)
}

type Null struct {
	defaultObject
}

var _ Object = (*Null)(nil)

func NewNull() *Null {
	return &Null{}
}

func (n *Null) Clone() Object {
	return &Null{}
}

func (n *Null) Type() ObjectType {
	return NullType
}

func (n *Null) String() string {
	return "null"
}

func (n *Null) IsNull() bool {
	return true
}

func (n *Null) Raw() any {
	return nil
}

type List struct {
	defaultObject
	items []Object
}

var _ Object = (*List)(nil)
var _ lenable = (*List)(nil)

func NewList(items []Object) *List {
	return &List{items: items}
}

func (l *List) Clone() Object {
	clonedItems := make([]Object, len(l.items))
	for i, item := range l.items {
		clonedItems[i] = item.Clone()
	}
	return &List{items: clonedItems}
}

func (l *List) Type() ObjectType {
	return ListType
}

func (l *List) String() string {
	var b bytes.Buffer
	b.WriteByte('[')

	for i, item := range l.items {
		if i > 0 {
			b.WriteString(", ")
		}
		b.WriteString(item.String())
	}

	b.WriteByte(']')

	return b.String()
}

func (l *List) IsLenable() bool {
	return true
}

func (l *List) Len() int {
	return len(l.items)
}

func (l *List) IsList() bool {
	return true
}

func (l *List) Items() []Object {
	return l.items
}

func (l *List) GetAt(ix int) (Object, bool) {
	if ix < 0 || ix >= len(l.items) {
		return nil, false
	}
	return l.items[ix], true
}

func (l *List) Append(item Object) {
	l.items = append(l.items, item)
}

func (l *List) SetAt(ix int, item Object) bool {
	if ix < 0 || ix >= len(l.items) {
		return false
	}
	l.items[ix] = item
	return true
}

func (l *List) Raw() any {
	return l.items
}

type Function struct {
	defaultObject
	Name      string
	Args      []string
	Instrs    code.Instructions
	NumLocals int
}

var _ Object = (*Function)(nil)

func NewFunction(name string, args []string, instrs code.Instructions) *Function {
	return &Function{
		Name:   name,
		Args:   args,
		Instrs: instrs,
	}
}

func (f *Function) Type() ObjectType {
	return FunctionType
}

func (f *Function) Clone() Object {
	return f
}

func (f *Function) String() string {
	var b bytes.Buffer
	b.WriteString("fn ")
	b.WriteString(f.Name)
	b.WriteByte('(')

	for i, arg := range f.Args {
		if i > 0 {
			b.WriteString(", ")
		}
		b.WriteString(arg)
	}

	b.WriteByte(')')

	return b.String()
}

func (f *Function) IsFunction() bool {
	return true
}

func (f *Function) Raw() any {
	return f
}

type Regexp struct {
	defaultObject
	pat string
	re  *regexp.Regexp
}

var _ Object = (*Regexp)(nil)

func (r *Regexp) Type() ObjectType {
	return RegexpType
}

func (r *Regexp) Clone() Object {
	return r
}

func (r *Regexp) String() string {
	return "Regexp(" + r.pat + ")"
}

func (r *Regexp) IsRegexp() bool {
	return true
}

func (r *Regexp) Raw() any {
	return r.pat
}

func NewRegexp(pat string) (*Regexp, error) {
	re, err := regexp.Compile(pat)
	if err != nil {
		return nil, err
	}
	return &Regexp{
		re:  re,
		pat: pat,
	}, nil
}

func (r *Regexp) Match(s string) bool {
	// TODO: optimize to skip capturing groups: these are not needed
	_, ok := r.re.MatchString(s)
	return ok
}

func (r *Regexp) Capture(s string) ([]string, bool) {
	return r.re.MatchString(s)
}

type Environment struct {
	Globals []Object
	Consts  []Object
	Store   map[string]Object
}

func NewEnvironment() *Environment {
	return &Environment{
		Globals: make([]Object, 0),
		Consts:  make([]Object, 0),
		Store:   make(map[string]Object),
	}
}

type Closure struct {
	defaultObject
	Fn     *Function
	Free   []Object
	Consts []Object
	Env    *Environment
	Module *Module
}

var _ Object = (*Closure)(nil)

func NewClosure(fn *Function, free []Object, env *Environment, mod *Module) *Closure {
	return &Closure{
		Fn:     fn,
		Free:   free,
		Env:    env,
		Module: mod,
	}
}

func (c *Closure) Type() ObjectType {
	return ClosureType
}

func (c *Closure) Clone() Object {
	return c
}

func (c *Closure) String() string {
	return "closure {" + c.Fn.String() + "}"
}

func (c *Closure) IsFunction() bool {
	return true
}

func (c *Closure) Raw() any {
	return c
}
