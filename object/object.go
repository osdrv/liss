package object

import "strconv"

type ObjectType uint8

const (
	_ ObjectType = iota
	IntegerType
	FloatType
	BoolType
	StringType
	NullType
	FunctionType
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
	default:
		return "Unknown"
	}
}

type Object interface {
	Type() ObjectType
	String() string
	IsNumeric() bool
	IsNull() bool
}

type nonNull struct{}

func (n *nonNull) IsNull() bool {
	return false
}

type Numeric interface {
	Int64() int64
	Float64() float64
}

type numeric struct{}

func (n numeric) IsNumeric() bool {
	return true
}

type nonNumeric struct{}

func (n *nonNumeric) IsNumeric() bool {
	return false
}

type Integer struct {
	numeric
	nonNull
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

func (i *Integer) Int64() int64 {
	return i.Value
}

func (i *Integer) Float64() float64 {
	return float64(i.Value)
}

type Float struct {
	numeric
	nonNull
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

type Bool struct {
	nonNumeric
	nonNull
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

type String struct {
	nonNumeric
	nonNull
	Value string
}

var _ Object = (*String)(nil)

func NewString(value string) *String {
	return &String{Value: value}
}

func (s *String) Type() ObjectType {
	return StringType
}

func (s *String) String() string {
	return s.Value
}

type Null struct {
	nonNumeric
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
