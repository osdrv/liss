package ast

type Node interface {
	String() string
	expressionNode()
}
