package ast

import (
	"bytes"
	"fmt"
	"osdrv/liss/token"
	"strings"
)

type BlockExpression struct {
	Nodes []Node
}

var _ Node = (*BlockExpression)(nil)

func NewBlockExpression(nodes []Node) *BlockExpression {
	return &BlockExpression{
		Nodes: nodes,
	}
}

func (e *BlockExpression) String() string {
	var b bytes.Buffer

	b.WriteByte('(')
	for i, op := range e.Nodes {
		if i > 0 {
			b.WriteByte(' ')
		}
		b.WriteString(op.String())
	}
	b.WriteByte(')')

	return b.String()
}

func (e *BlockExpression) expressionNode() {}

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
	Operands []Node
}

var _ Node = (*OperatorExpr)(nil)

func NewOperatorExpr(tok token.Token, operands []Node) (*OperatorExpr, error) {
	if op, ok := tokenToOperator[tok.Type]; ok {
		return &OperatorExpr{
			Token:    tok,
			Operator: op,
			Operands: operands,
		}, nil
	}
	return nil, fmt.Errorf("unexpected token type: %s", tok.Type.String())
}

func (e *OperatorExpr) String() string {
	var b strings.Builder
	b.WriteByte('(')
	if s, ok := operatorToString[e.Operator]; ok {
		b.WriteString(s)
	} else {
		b.WriteString(fmt.Sprintf("Operator(%d)", e.Operator))
	}
	if len(e.Operands) > 0 {
		for _, arg := range e.Operands {
			b.WriteByte(' ')
			b.WriteString(arg.String())
		}
	}
	b.WriteByte(')')
	return b.String()
}

func (e *OperatorExpr) expressionNode() {}

type CondExpression struct {
	Token token.Token
	Cond  Node
	Then  Node
	Else  Node
}

var _ Node = (*CondExpression)(nil)

func NewCondExpression(tok token.Token, cond Node, th Node, el Node) (*CondExpression, error) {
	return &CondExpression{
		Cond:  cond,
		Token: tok,
		Then:  th,
		Else:  el,
	}, nil
}

func (e *CondExpression) String() string {
	var b bytes.Buffer

	b.WriteString("cond ")
	b.WriteString(e.Then.String())
	if e.Else != nil {
		b.WriteString(" ")
		b.WriteString(e.Else.String())
	}

	return b.String()
}

func (e *CondExpression) expressionNode() {}

type ImportExpression struct {
	Token token.Token
	Ref   Node
}

var _ Node = (*ImportExpression)(nil)

func NewImportExpression(tok token.Token, ref Node) (*ImportExpression, error) {
	return &ImportExpression{
		Token: tok,
		Ref:   ref,
	}, nil
}

func (e *ImportExpression) String() string {
	return "import " + e.Ref.String()
}

func (e *ImportExpression) expressionNode() {}

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

type CallExpression struct {
	Token  token.Token
	Callee Node
	Args   []Node
}

var _ Node = (*CallExpression)(nil)

func NewCallExpression(tok token.Token, callee Node, args []Node) (*CallExpression, error) {
	return &CallExpression{
		Token:  tok,
		Callee: callee,
		Args:   args,
	}, nil
}

func (e *CallExpression) String() string {
	var b bytes.Buffer
	b.WriteByte('(')
	b.WriteString(e.Callee.String())
	for _, arg := range e.Args {
		b.WriteByte(' ')
		b.WriteString(arg.String())
	}
	b.WriteByte(')')
	return b.String()
}

func (e *CallExpression) expressionNode() {}

type ListExpression struct {
	Token token.Token
	Items []Node
}

var _ Node = (*ListExpression)(nil)

func NewListExpression(tok token.Token, items []Node) (*ListExpression, error) {
	return &ListExpression{
		Token: tok,
		Items: items,
	}, nil
}

func (e *ListExpression) String() string {
	var b bytes.Buffer
	b.WriteByte('[')
	for i, item := range e.Items {
		if i > 0 {
			b.WriteByte(' ')
		}
		b.WriteString(item.String())
	}
	b.WriteByte(']')
	return b.String()
}

func (e *ListExpression) expressionNode() {}
