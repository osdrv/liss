package ast

import "bytes"

type Program struct {
	Exprs []*Expression
}

var _ Node = (*Program)(nil)

func NewProgram(nodes []Node) *Program {
	exprs := make([]*Expression, 0, len(nodes))
	for _, node := range nodes {
		var expr *Expression
		if e, ok := node.(*Expression); ok {
			expr = e
		} else {
			expr = NewExpression([]Node{node})
		}
		exprs = append(exprs, expr)
	}
	return &Program{
		Exprs: exprs,
	}
}

func (p *Program) String() string {
	var b bytes.Buffer

	for _, node := range p.Exprs {
		b.WriteString(node.String())
		b.WriteByte('\n')
	}

	return b.String()
}

func (p *Program) expressionNode() {}
