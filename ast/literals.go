package ast

import (
	"fmt"
	"osdrv/liss/token"
	"strconv"
)

type NullLiteral struct {
	emptyNode
	Tok token.Token
}

var _ Node = (*NullLiteral)(nil)

func NewNullLiteral(tok token.Token) (*NullLiteral, error) {
	if tok.Type != token.Null {
		return nil, fmt.Errorf("expected token type Null, got %s", tok.Type.String())
	}

	return &NullLiteral{Tok: tok}, nil
}

func (n *NullLiteral) Token() token.Token {
	return n.Tok
}

func (n *NullLiteral) String() string {
	return "null"
}

type IntegerLiteral struct {
	emptyNode
	Tok   token.Token
	Value int64
}

var _ Node = (*IntegerLiteral)(nil)

func NewIntegerLiteral(tok token.Token) (*IntegerLiteral, error) {
	if tok.Type != token.Numeric {
		return nil, fmt.Errorf("expected token type Numeric, got %s", tok.Type.String())
	}

	v, err := strconv.ParseInt(tok.Literal, 10, 64)
	if err != nil {
		return nil, err
	}
	return &IntegerLiteral{Tok: tok, Value: v}, nil
}

func (i *IntegerLiteral) Token() token.Token {
	return i.Tok
}

func (i *IntegerLiteral) String() string {
	return strconv.FormatInt(i.Value, 10)
}

type FloatLiteral struct {
	emptyNode
	Tok   token.Token
	Value float64
}

var _ Node = (*FloatLiteral)(nil)

func NewFloatLiteral(tok token.Token) (*FloatLiteral, error) {
	if tok.Type != token.Numeric {
		return nil, fmt.Errorf("expected token type Numeric, got %s", tok.Type.String())
	}

	v, err := strconv.ParseFloat(tok.Literal, 64)
	if err != nil {
		return nil, err
	}
	return &FloatLiteral{Tok: tok, Value: v}, nil
}

func (f *FloatLiteral) Token() token.Token {
	return f.Tok
}

func (f *FloatLiteral) String() string {
	return strconv.FormatFloat(f.Value, 'f', -1, 64)
}

type StringLiteral struct {
	emptyNode
	Tok   token.Token
	Value string
}

var _ Node = (*StringLiteral)(nil)

func NewStringLiteral(tok token.Token) (*StringLiteral, error) {
	if tok.Type != token.String {
		return nil, fmt.Errorf("expected token type String, got %s", tok.Type.String())
	}

	return &StringLiteral{Tok: tok, Value: tok.Literal}, nil
}

func (s *StringLiteral) Token() token.Token {
	return s.Tok
}

func (s *StringLiteral) String() string {
	return s.Value
}

type BooleanLiteral struct {
	emptyNode
	Tok   token.Token
	Value bool
}

var _ Node = (*BooleanLiteral)(nil)

func NewBooleanLiteral(tok token.Token) (*BooleanLiteral, error) {
	if tok.Type != token.True && tok.Type != token.False {
		return nil, fmt.Errorf("expected token type Boolean, got %s", tok.Type.String())
	}

	return &BooleanLiteral{Tok: tok, Value: tok.Type == token.True}, nil
}

func (b *BooleanLiteral) Token() token.Token {
	return b.Tok
}

func (b *BooleanLiteral) String() string {
	if b.Value {
		return "true"
	}
	return "false"
}
