package ast

import "bytes"

type Node interface {
	String() string
}

type Expression struct {
	Operands []Node
}

var _ Node = (*Expression)(nil)

func NewExpression(operands ...Node) *Expression {
	return &Expression{
		Operands: operands,
	}
}

func (e *Expression) String() string {
	var b bytes.Buffer

	b.WriteByte('(')
	for i, op := range e.Operands {
		if i > 0 {
			b.WriteByte(' ')
		}
		b.WriteString(op.String())
	}
	b.WriteByte(')')

	return b.String()
}
