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

type Object interface {
	Type() ObjectType
	String() string
}

type Integer struct {
	Value int64
}

var _ Object = (*Integer)(nil)

func NewInteger(value int64) *Integer {
	return &Integer{Value: value}
}

func (i *Integer) Type() ObjectType {
	return IntegerType
}

func (i *Integer) String() string {
	return strconv.FormatInt(i.Value, 10)
}

type Float struct {
	Value float64
}

var _ Object = (*Float)(nil)

func NewFloat(value float64) *Float {
	return &Float{Value: value}
}

func (f *Float) Type() ObjectType {
	return FloatType
}

func (f *Float) String() string {
	return strconv.FormatFloat(f.Value, 'f', -1, 64)
}

type Bool struct {
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
