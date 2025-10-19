package object

import (
	"bytes"
	"fmt"
	"osdrv/liss/code"
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
	ListType
	DictionaryType
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
	case ListType:
		return "List"
	case DictionaryType:
		return "Dictionary"
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

type lenable interface {
	Len() int
}

type Numeric interface {
	Int64() int64
	Float64() float64
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

type Float struct {
	defaultObject
	Value float64
}

var _ Object = (*Float)(nil)
var _ Numeric = (*Float)(nil)

func NewFloat(value float64) *Float {
	return &Float{Value: value}
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

func (f *Float) IsNumeric() bool {
	return true
}

type Bool struct {
	defaultObject
	Value bool
}

var _ Object = (*Bool)(nil)

func NewBool(value bool) *Bool {
	return &Bool{Value: value}
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

type String struct {
	defaultObject
	Value string
}

var _ Object = (*String)(nil)
var _ lenable = (*String)(nil)

func NewString(value string) *String {
	return &String{Value: value}
}

func (s *String) Type() ObjectType {
	return StringType
}

func (s *String) String() string {
	return s.Value
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

type Null struct {
	defaultObject
}

var _ Object = (*Null)(nil)

func NewNull() *Null {
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

type List struct {
	defaultObject
	items []Object
}

var _ Object = (*List)(nil)
var _ lenable = (*List)(nil)

func NewList(items []Object) *List {
	return &List{items: items}
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

type Dictionary struct {
	defaultObject
	items map[any]Object
}

var _ Object = (*Dictionary)(nil)
var _ lenable = (*Dictionary)(nil)

func NewDictionary(items map[any]Object) *Dictionary {
	return &Dictionary{items: items}
}

func (d *Dictionary) Type() ObjectType {
	return DictionaryType
}

func (d *Dictionary) String() string {
	var b bytes.Buffer
	b.WriteByte('{')

	first := true
	for key, value := range d.items {
		if !first {
			b.WriteString(", ")
		}
		first = false
		var strkey string
		if _, ok := key.(fmt.Stringer); ok {
			strkey = key.(fmt.Stringer).String()
		}
		b.WriteString(strconv.Quote(strkey))
		b.WriteString(": ")
		b.WriteString(value.String())
	}

	b.WriteByte('}')

	return b.String()
}

func (d *Dictionary) IsLenable() bool {
	return true
}

func (d *Dictionary) Len() int {
	return len(d.items)
}

func (d *Dictionary) IsDictionary() bool {
	return true
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
