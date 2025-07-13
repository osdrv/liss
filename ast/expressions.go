package ast

import (
	"bytes"
	"fmt"
	"osdrv/liss/token"
)

type Expression struct {
	Operands []Node
}

var _ Node = (*Expression)(nil)

func NewExpression(operands []Node) *Expression {
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

func (e *Expression) expressionNode() {}

type IdentifierExpr struct {
	Token token.Token
	Name  string
}

var _ Node = (*IdentifierExpr)(nil)

func NewIdentifierExpr(tok token.Token) (*IdentifierExpr, error) {
	if tok.Type != token.Identifier {
		return nil, fmt.Errorf("expected token type Identifier, got %s", tok.Type.String())
	}
	return &IdentifierExpr{
		Token: tok,
		Name:  tok.Literal,
	}, nil
}

func (e *IdentifierExpr) String() string {
	return e.Name
}

func (e *IdentifierExpr) expressionNode() {}

type OperatorExpr struct {
	Token    token.Token
	Operator Operator
}

var _ Node = (*OperatorExpr)(nil)

func NewOperatorExpr(tok token.Token) (*OperatorExpr, error) {
	if op, ok := tokenToOperator[tok.Type]; ok {
		return &OperatorExpr{
			Token:    tok,
			Operator: op,
		}, nil
	}
	return nil, fmt.Errorf("unexpected token type: %s", tok.Type.String())
}

func (e *OperatorExpr) String() string {
	if s, ok := operatorToString[e.Operator]; ok {
		return s
	}
	return fmt.Sprintf("Operator(%d)", e.Operator)
}

func (e *OperatorExpr) expressionNode() {}
