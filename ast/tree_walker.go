package ast

type TreeWalker struct {
	node Node
}

func NewTreeWalker(node Node) *TreeWalker {
	return &TreeWalker{
		node: node,
	}
}

func (tw *TreeWalker) CollectNodes(fn func(node *Node) bool) []Node {
	var walk func(acc []Node, node Node) []Node
	walk = func(acc []Node, node Node) []Node {
		if fn(&node) {
			acc = append(acc, node)
		}
		for _, child := range node.Children() {
			acc = walk(acc, child)
		}
		return acc
	}
	acc := make([]Node, 0)
	return walk(acc, tw.node)
}
