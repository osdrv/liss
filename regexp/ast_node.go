package regexp

type NodeType uint8

const (
	NodeTypeLiteral NodeType = iota
	NodeTypeAny
	NodeTypeClass
	NodeTypeConcat
	NodeTypeAlternation
	NodeTypeStar
	NodeTypePlus
	NodeTypeQuestion
	NodeTypeGroup
	NodeTypeCapture
	NodeTypeStartOfString
	NodeTypeEndOfString
)

type ASTNode struct {
	Type     NodeType
	Value    string
	Children []*ASTNode
}
