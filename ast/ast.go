package ast

import "osdrv/liss/token"

type Node interface {
	String() string
	expressionNode()
}

type Operator uint8

const (
	OperatorVoid Operator = iota
	OperatorPlus
	OperatorMinus
	OperatorMultiply
	OperatorDivide
	OperatorModulus

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
