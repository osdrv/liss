package token

import "fmt"

type TokenType uint8

func (t TokenType) String() string {
	if str, ok := tokenToString[t]; ok {
		return str
	}
	switch t {
	case String:
		return "String"
	case Numeric:
		return "Numeric"
	case Identifier:
		return "Identifier"
	default:
		return fmt.Sprintf("Unknown(%d)", t)
	}
}

const (
	Void TokenType = iota
	EOF
	Error
	Raise

	LParen
	RParen
	LBracket
	RBracket
	Dot

	Plus
	Minus
	Multiply
	Divide
	Modulus

	Equal
	NotEqual
	LessThan
	LessThanOrEqual
	GreaterThan
	GreaterThanOrEqual
	And
	Or
	Not

	Fn
	Identifier
	Numeric
	String
	Accessor
	True
	False
	Null
	Cond
	Let
	Import
	As
	Breakpoint
)

var (
	tokenToString = map[TokenType]string{
		EOF:   "",
		Raise: "raise",

		LParen:   "(",
		RParen:   ")",
		LBracket: "[",
		RBracket: "]",
		Dot:      ".",

		Plus:     "+",
		Minus:    "-",
		Multiply: "*",
		Divide:   "/",
		Modulus:  "%",

		Equal:              "=",
		NotEqual:           "!=",
		LessThan:           "<",
		LessThanOrEqual:    "<=",
		GreaterThan:        ">",
		GreaterThanOrEqual: ">=",
		And:                "&",
		Or:                 "|",
		Not:                "!",

		Fn:         "fn",
		True:       "true",
		False:      "false",
		Null:       "null",
		Cond:       "cond",
		Let:        "let",
		Import:     "import",
		As:         "as",
		Breakpoint: "breakpoint",
	}
)

var (
	Keywords = map[string]TokenType{
		"fn":         Fn,
		"true":       True,
		"false":      False,
		"null":       Null,
		"cond":       Cond,
		"let":        Let,
		"and":        And,
		"or":         Or,
		"not":        Not,
		"plus":       Plus,
		"minus":      Minus,
		"mult":       Multiply,
		"div":        Divide,
		"mod":        Modulus,
		"import":     Import,
		"as":         As,
		"breakpoint": Breakpoint,
		"raise!":     Raise,
	}
)

type Location struct {
	Line   int
	Column int
}

type Token struct {
	Type     TokenType
	Literal  string
	Location Location
}

func (t Token) String() string {
	if str, ok := tokenToString[t.Type]; ok {
		return str
	}
	return t.Literal
}

func (t Token) Pos() string {
	return fmt.Sprintf("Line: %d, Column: %d", t.Location.Line, t.Location.Column)
}
