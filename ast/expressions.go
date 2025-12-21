package ast

import (
	"bytes"
	"fmt"
	"osdrv/liss/token"
	"strings"
)

type BlockExpression struct {
	Tok   token.Token
	Nodes []Node
}

var _ Node = (*BlockExpression)(nil)

func NewBlockExpression(tok token.Token, nodes []Node) *BlockExpression {
	return &BlockExpression{
		Tok:   tok,
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

func (e *BlockExpression) Children() []Node {
	return e.Nodes
}

func (e *BlockExpression) Token() token.Token {
	return e.Tok
}

func (e *BlockExpression) expressionNode() {}

type IdentifierExpr struct {
	emptyNode
	Tok    token.Token
	Module string
	Name   string
}

var _ Node = (*IdentifierExpr)(nil)

func NewIdentifierExpr(tok token.Token) (*IdentifierExpr, error) {
	if tok.Type != token.Identifier {
		return nil, fmt.Errorf("expected token type Identifier, got %s", tok.Type.String())
	}
	module := ""
	name := tok.Literal
	if strings.Contains(name, ":") {
		parts := strings.Split(name, ":")
		if len(parts) > 2 {
			return nil, fmt.Errorf("invalid identifier format: %q", name)
		}
		if len(parts) == 2 {
			module = parts[0]
			name = parts[1]
		}
	}
	return &IdentifierExpr{
		Tok:    tok,
		Module: module,
		Name:   name,
	}, nil
}

func (e *IdentifierExpr) Token() token.Token {
	return e.Tok
}

func (e *IdentifierExpr) FullName() string {
	var sb strings.Builder
	if e.Module != "" {
		sb.WriteString(e.Module)
		sb.WriteByte(':')
	}
	sb.WriteString(e.Name)
	return sb.String()
}

func (e *IdentifierExpr) HasModule() bool {
	return e.Module != ""
}

func (e *IdentifierExpr) String() string {
	return e.FullName()
}

type OperatorExpr struct {
	Tok      token.Token
	Operator Operator
	Operands []Node
}

var _ Node = (*OperatorExpr)(nil)

func NewOperatorExpr(tok token.Token, operands []Node) (*OperatorExpr, error) {
	if op, ok := tokenToOperator[tok.Type]; ok {
		return &OperatorExpr{
			Tok:      tok,
			Operator: op,
			Operands: operands,
		}, nil
	}
	return nil, fmt.Errorf("unexpected token type: %s", tok.Type.String())
}

func (e *OperatorExpr) Token() token.Token {
	return e.Tok
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

func (e *OperatorExpr) Children() []Node {
	return e.Operands
}

func (e *OperatorExpr) expressionNode() {}

type CondExpression struct {
	Tok  token.Token
	Cond Node
	Then Node
	Else Node
}

var _ Node = (*CondExpression)(nil)

func NewCondExpression(tok token.Token, cond Node, th Node, el Node) (*CondExpression, error) {
	return &CondExpression{
		Cond: cond,
		Tok:  tok,
		Then: th,
		Else: el,
	}, nil
}

func (e *CondExpression) Token() token.Token {
	return e.Tok
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

func (e *CondExpression) Children() []Node {
	if e.Else != nil {
		return []Node{e.Cond, e.Then, e.Else}
	}
	return []Node{e.Cond, e.Then}
}

func (e *CondExpression) expressionNode() {}

type ImportExpression struct {
	emptyNode
	Tok     token.Token
	Ref     Node
	Symbols *ListExpression
	Alias   *IdentifierExpr
}

var _ Node = (*ImportExpression)(nil)

func NewImportExpression(tok token.Token, ref Node, symbols *ListExpression, alias *IdentifierExpr) (*ImportExpression, error) {
	return &ImportExpression{
		Tok:     tok,
		Ref:     ref,
		Symbols: symbols,
		Alias:   alias,
	}, nil
}

func (e *ImportExpression) Token() token.Token {
	return e.Tok
}

func (e *ImportExpression) String() string {
	var sb strings.Builder
	sb.WriteString("import ")
	sb.WriteString(e.Ref.String())
	if e.Symbols != nil {
		sb.WriteString(" [")
		for i, sym := range e.Symbols.Items {
			if i > 0 {
				sb.WriteByte(' ')
			}
			sb.WriteString(sym.String())
		}
		sb.WriteByte(']')
	}
	return sb.String()
}

type LetExpression struct {
	emptyNode
	Tok        token.Token
	Identifier *IdentifierExpr
	Value      Node
}

var _ Node = (*LetExpression)(nil)

func NewLetExpression(tok token.Token, id *IdentifierExpr, value Node) (*LetExpression, error) {
	return &LetExpression{
		Tok:        tok,
		Identifier: id,
		Value:      value,
	}, nil
}

func (e *LetExpression) Token() token.Token {
	return e.Tok
}

func (e *LetExpression) String() string {
	var b bytes.Buffer

	b.WriteString("let ")
	b.WriteString(e.Identifier.String())
	b.WriteString(" = ")
	b.WriteString(e.Value.String())

	return b.String()
}

type FunctionExpression struct {
	Tok  token.Token
	Name *IdentifierExpr // Optional, can be nil
	Args []*IdentifierExpr
	Body []Node
}

var _ Node = (*FunctionExpression)(nil)

func NewFunctionExpression(tok token.Token, name *IdentifierExpr, args []*IdentifierExpr, body []Node) (*FunctionExpression, error) {
	return &FunctionExpression{
		Tok:  tok,
		Name: name,
		Args: args,
		Body: body,
	}, nil
}

func (e *FunctionExpression) Token() token.Token {
	return e.Tok
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

func (e *FunctionExpression) Children() []Node {
	nodes := make([]Node, 0, len(e.Args)+len(e.Body))
	for _, arg := range e.Args {
		nodes = append(nodes, arg)
	}
	nodes = append(nodes, e.Body...)
	return nodes
}

func (e *FunctionExpression) expressionNode() {}

type CallExpression struct {
	Tok    token.Token
	Callee Node
	Args   []Node
}

var _ Node = (*CallExpression)(nil)

func NewCallExpression(tok token.Token, callee Node, args []Node) (*CallExpression, error) {
	return &CallExpression{
		Tok:    tok,
		Callee: callee,
		Args:   args,
	}, nil
}

func (e *CallExpression) Token() token.Token {
	return e.Tok
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

func (e *CallExpression) Children() []Node {
	nodes := make([]Node, 0, 1+len(e.Args))
	nodes = append(nodes, e.Callee)
	nodes = append(nodes, e.Args...)
	return nodes
}

func (e *CallExpression) expressionNode() {}

type ListExpression struct {
	Tok   token.Token
	Items []Node
}

var _ Node = (*ListExpression)(nil)

func NewListExpression(tok token.Token, items []Node) (*ListExpression, error) {
	return &ListExpression{
		Tok:   tok,
		Items: items,
	}, nil
}

func (e *ListExpression) Token() token.Token {
	return e.Tok
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

func (e *ListExpression) Children() []Node {
	return e.Items
}

func (e *ListExpression) expressionNode() {}

type BreakpointExpression struct {
	emptyNode
	Tok token.Token
}

var _ Node = (*BreakpointExpression)(nil)

func NewBreakpointExpression(tok token.Token) (*BreakpointExpression, error) {
	return &BreakpointExpression{
		Tok: tok,
	}, nil
}

func (e *BreakpointExpression) Token() token.Token {
	return e.Tok
}

func (e *BreakpointExpression) String() string {
	return "breakpoint"
}

type RaiseExpression struct {
	Tok  token.Token
	Expr Node
}

var _ Node = (*RaiseExpression)(nil)

func NewRaiseExpression(tok token.Token, expr Node) (*RaiseExpression, error) {
	return &RaiseExpression{
		Tok:  tok,
		Expr: expr,
	}, nil
}

func (e *RaiseExpression) Token() token.Token {
	return e.Tok
}

func (e *RaiseExpression) String() string {
	return fmt.Sprintf("(raise %s)", e.Expr.String())
}

func (e *RaiseExpression) Children() []Node {
	return []Node{e.Expr}
}

func (e *RaiseExpression) expressionNode() {}

type CaseExpression struct {
	When Node // When When is nil, it represents the default case aka match all
	Then Node
}

func NewCaseExpression(when Node, then Node) (*CaseExpression, error) {
	return &CaseExpression{
		When: when,
		Then: then,
	}, nil
}

func (ce *CaseExpression) String() string {
	var b bytes.Buffer
	b.WriteByte('[')
	if ce.When != nil {
		b.WriteString(ce.When.String())
	} else {
		b.WriteByte('*')
	}
	b.WriteByte(' ')
	b.WriteString(ce.Then.String())
	b.WriteByte(']')
	return b.String()
}

type SwitchExpression struct {
	Tok     token.Token
	Expr    Node
	Cases   []*CaseExpression
	Default *CaseExpression
}

var _ Node = (*SwitchExpression)(nil)

func NewSwitchExpression(tok token.Token, expr Node, cases []*CaseExpression, def *CaseExpression) (*SwitchExpression, error) {
	e := &SwitchExpression{
		Tok:     tok,
		Expr:    expr,
		Cases:   cases,
		Default: def,
	}
	if expr == nil {
		e.Expr = &BooleanLiteral{
			Tok:   tok,
			Value: true,
		}
	}
	return e, nil
}

func (e *SwitchExpression) Token() token.Token {
	return e.Tok
}

func (e *SwitchExpression) String() string {
	var b bytes.Buffer
	b.WriteString("switch ")
	for _, c := range e.Cases {
		b.WriteString(fmt.Sprintf("\n  %s", c.String()))
	}
	if e.Default != nil {
		b.WriteString(fmt.Sprintf("\n  %s", e.Default.String()))
	}
	return b.String()
}

func (e *SwitchExpression) Children() []Node {
	nodes := make([]Node, 0, 2*len(e.Cases)+1)
	for _, c := range e.Cases {
		nodes = append(nodes, c.When)
		nodes = append(nodes, c.Then)
	}
	if e.Default != nil {
		nodes = append(nodes, e.Default.Then)
	}
	return nodes
}

func (e *SwitchExpression) expressionNode() {}

type TryExpression struct {
	Tok  token.Token
	Body Node
}

var _ Node = (*TryExpression)(nil)

func NewTryExpression(tok token.Token, body Node) (*TryExpression, error) {
	return &TryExpression{
		Tok:  tok,
		Body: body,
	}, nil
}

func (e *TryExpression) Token() token.Token {
	return e.Tok
}

func (e *TryExpression) String() string {
	return fmt.Sprintf("(try %s)", e.Body.String())
}

func (e *TryExpression) Children() []Node {
	return []Node{e.Body}
}

func (e *TryExpression) expressionNode() {}
