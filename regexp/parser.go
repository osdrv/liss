package regexp

import (
	"fmt"
	"strings"
)

type parser struct {
	input []rune
	pos   int
}

func (p *parser) peek() rune {
	if p.pos >= len(p.input) {
		return 0
	}
	return p.input[p.pos]
}

func (p *parser) consume() rune {
	if p.pos >= len(p.input) {
		return 0
	}
	ch := p.input[p.pos]
	p.pos++
	return ch
}

// Handles literals and groups
func (p *parser) parseAtom() (*ASTNode, error) {
	if p.peek() == '^' {
		p.consume()
		return &ASTNode{Type: NodeTypeStartOfString}, nil
	}
	if p.peek() == '$' {
		p.consume()
		return &ASTNode{Type: NodeTypeEndOfString}, nil
	}
	if p.peek() == '.' {
		p.consume() // consume '.'
		return &ASTNode{Type: NodeTypeAny}, nil
	}
	if p.peek() == '\\' {
		p.consume() // consume '\'
		r := p.peek()
		if r == 0 {
			return nil, fmt.Errorf("Unexpected end of input after escape character")
		}
		p.consume()
		switch r {
		case 'd', 'D', 'w', 'W', 's', 'S':
			return &ASTNode{Type: NodeTypeClass, Value: string(r)}, nil
		default:
			return &ASTNode{Type: NodeTypeLiteral, Value: string(r)}, nil
		}
	}
	if p.peek() == '(' {
		p.consume() // consume '('
		isCapture := true
		node, err := p.parseAlternate()
		if err != nil {
			return nil, err
		}
		if p.peek() != ')' {
			return nil, fmt.Errorf("Missing closing ')'")
		}
		p.consume()
		if isCapture {
			return &ASTNode{Type: NodeTypeCapture, Children: []*ASTNode{node}}, nil
		}
		return node, nil
	}
	start := p.pos
	for {
		r := p.peek()
		if r == 0 || strings.ContainsRune("()|*+?.\\^$", r) {
			break
		}
		p.consume()
	}
	if start == p.pos {
		if p.pos < len(p.input) {
			return nil, fmt.Errorf("Unexpected character: '%c'", p.peek())
		}
		return nil, fmt.Errorf("Unexpected end of input")
	}
	return &ASTNode{Type: NodeTypeLiteral, Value: string(p.input[start:p.pos])}, nil
}

func (p *parser) parsePostfix() (*ASTNode, error) {
	node, err := p.parseAtom()
	if err != nil {
		return nil, err
	}
	for {
		r := p.peek()
		var op NodeType
		switch r {
		case '*':
			op = NodeTypeStar
		case '+':
			op = NodeTypePlus
		case '?':
			op = NodeTypeQuestion
		default:
			return node, nil
		}
		p.consume()
		node = &ASTNode{Type: op, Children: []*ASTNode{node}}
	}
}

// Handles concatenation
func (p *parser) parseConcat() (*ASTNode, error) {
	var nodes []*ASTNode
	for {
		r := p.peek()
		if r == 0 || r == ')' || r == '|' {
			break
		}
		node, err := p.parsePostfix()
		if err != nil {
			return nil, err
		}
		nodes = append(nodes, node)
	}
	if len(nodes) == 1 {
		return nodes[0], nil
	}
	return &ASTNode{Type: NodeTypeConcat, Children: nodes}, nil
}

func (p *parser) parseAlternate() (*ASTNode, error) {
	var nodes []*ASTNode
	node, err := p.parseConcat()
	if err != nil {
		return nil, err
	}
	nodes = append(nodes, node)
	for p.peek() == '|' {
		p.consume() // consume '|'
		node, err := p.parseConcat()
		if err != nil {
			return nil, err
		}
		nodes = append(nodes, node)
	}
	if len(nodes) == 1 {
		return nodes[0], nil
	}
	return &ASTNode{Type: NodeTypeAlternation, Children: nodes}, nil
}
