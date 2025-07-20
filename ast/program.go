package ast

import "bytes"

type Program struct {
	Nodes []Node
}

var _ Node = (*Program)(nil)

func NewProgram(nodes []Node) *Program {
	return &Program{
		Nodes: nodes,
	}
}

func (p *Program) String() string {
	var b bytes.Buffer

	for _, node := range p.Nodes {
		b.WriteString(node.String())
		b.WriteByte('\n')
	}

	return b.String()
}

func (p *Program) expressionNode() {}
