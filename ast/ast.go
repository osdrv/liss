package ast

import (
	"fmt"
	"osdrv/liss/token"
)

type Node interface {
	Token() token.Token
	String() string
	Children() []Node
	expressionNode()
}

type emptyNode struct{}

func (n *emptyNode) expressionNode() {}

func (n *emptyNode) Children() []Node {
	return nil
}

type Operator uint8

const (
	OperatorVoid Operator = iota
	OperatorPlus
	OperatorMinus
	OperatorMultiply
	OperatorDivide
	OperatorModulus

	OperatorBitwiseAnd
	OperatorBitwiseOr
	OperatorBitwiseXor
	OperatorBitwiseNot
	OperatorBitwiseShiftLeft
	OperatorBitwiseShiftRight

	OperatorEqual
	OperatorNotEqual
	OperatorLessThan
	OperatorLessThanOrEqual
	OperatorGreaterThan
	OperatorGreaterThanOrEqual

	OperatorAnd
	OperatorOr
	OperatorNot
)

var tokenToOperator = map[token.TokenType]Operator{
	token.Plus:     OperatorPlus,
	token.Minus:    OperatorMinus,
	token.Multiply: OperatorMultiply,
	token.Divide:   OperatorDivide,
	token.Modulus:  OperatorModulus,

	token.BAnd:        OperatorBitwiseAnd,
	token.BOr:         OperatorBitwiseOr,
	token.BXor:        OperatorBitwiseXor,
	token.BNot:        OperatorBitwiseNot,
	token.BShiftLeft:  OperatorBitwiseShiftLeft,
	token.BShiftRight: OperatorBitwiseShiftRight,

	token.Equal:              OperatorEqual,
	token.NotEqual:           OperatorNotEqual,
	token.LessThan:           OperatorLessThan,
	token.LessThanOrEqual:    OperatorLessThanOrEqual,
	token.GreaterThan:        OperatorGreaterThan,
	token.GreaterThanOrEqual: OperatorGreaterThanOrEqual,
	token.And:                OperatorAnd,
	token.Or:                 OperatorOr,
	token.Not:                OperatorNot,
}

var operatorToString = map[Operator]string{
	OperatorPlus:     "+",
	OperatorMinus:    "-",
	OperatorMultiply: "*",
	OperatorDivide:   "/",
	OperatorModulus:  "%",

	OperatorBitwiseAnd:        "&&",
	OperatorBitwiseOr:         "||",
	OperatorBitwiseXor:        "^",
	OperatorBitwiseNot:        "~",
	OperatorBitwiseShiftLeft:  "<<",
	OperatorBitwiseShiftRight: ">>",

	OperatorEqual:              "=",
	OperatorNotEqual:           "!=",
	OperatorLessThan:           "<",
	OperatorLessThanOrEqual:    "<=",
	OperatorGreaterThan:        ">",
	OperatorGreaterThanOrEqual: ">=",

	OperatorAnd: "&",
	OperatorOr:  "|",
	OperatorNot: "!",
}

func (op Operator) String() string {
	if s, ok := operatorToString[op]; ok {
		return s
	}
	return fmt.Sprintf("Operator(%d)", op)
}
