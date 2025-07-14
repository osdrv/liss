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

type IfExpression struct {
	Token token.Token
	Cond  Node
	Then  Node
	Else  Node
}

var _ Node = (*IfExpression)(nil)

func NewIfExpression(tok token.Token, cond Node, th Node, el Node) (*IfExpression, error) {
	return &IfExpression{
		Cond:  cond,
		Token: tok,
		Then:  th,
		Else:  el,
	}, nil
}

func (e *IfExpression) String() string {
	var b bytes.Buffer

	b.WriteString("if ")
	b.WriteString(e.Then.String())
	if e.Else != nil {
		b.WriteString(" ")
		b.WriteString(e.Else.String())
	}

	return b.String()
}

func (e *IfExpression) expressionNode() {}

type LetExpression struct {
	Token      token.Token
	Identifier *IdentifierExpr
	Value      Node
}

var _ Node = (*LetExpression)(nil)

func NewLetExpression(tok token.Token, id *IdentifierExpr, value Node) (*LetExpression, error) {
	return &LetExpression{
		Token:      tok,
		Identifier: id,
		Value:      value,
	}, nil
}

func (e *LetExpression) String() string {
	var b bytes.Buffer

	b.WriteString("let ")
	b.WriteString(e.Identifier.String())
	b.WriteString(" = ")
	b.WriteString(e.Value.String())

	return b.String()
}

func (e *LetExpression) expressionNode() {}

type FunctionExpression struct {
	Token token.Token
	Name  *IdentifierExpr // Optional, can be nil
	Args  []*IdentifierExpr
	Body  []Node
}

var _ Node = (*FunctionExpression)(nil)

func NewFunctionExpression(tok token.Token, name *IdentifierExpr, args []*IdentifierExpr, body []Node) (*FunctionExpression, error) {
	return &FunctionExpression{
		Token: tok,
		Name:  name,
		Args:  args,
		Body:  body,
	}, nil
}

func (e *FunctionExpression) String() string {
	var b bytes.Buffer

	b.WriteString("fn ")
	if e.Name != nil {
		b.WriteString(e.Name.String())
		b.WriteByte(' ')
	}
	b.WriteByte('[')
	for i, arg := range e.Args {
		if i > 0 {
			b.WriteByte(' ')
		}
		b.WriteString(arg.String())
	}
	b.WriteByte(']')
	b.WriteByte('\n')
	for _, expr := range e.Body {
		b.WriteString("  ")
		b.WriteString(expr.String())
		b.WriteByte('\n')
	}

	return b.String()
}

func (e *FunctionExpression) expressionNode() {}
