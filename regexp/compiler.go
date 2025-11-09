package regexp

import "fmt"

type compiler struct {
	prog  *Prog
	nCaps int
}

func (c *compiler) addInst(inst Inst) int {
	idx := len(c.prog.Insts)
	c.prog.Insts = append(c.prog.Insts, inst)
	return idx
}

func (c *compiler) patch(ix int, out int) {
	c.prog.Insts[ix].Out = out
}

func (c *compiler) parchSplit(ix int, out int, out1 int) {
	c.prog.Insts[ix].Out = out
	c.prog.Insts[ix].Out1 = out1
}

// Compiles the AST recursively into a sequence of instructions.
// Returns the index of the first instruction of the compiled node.
func (c *compiler) compile(node *ASTNode, next int) int {
	switch node.Type {
	case NodeTypeLiteral:
		runes := []rune(node.Value)
		start := next
		// TODO: Optimize for literals longer than 1 rune
		for i := len(runes) - 1; i >= 0; i-- {
			inst := Inst{
				Op:   ReOpRune,
				Rune: runes[i],
				Out:  start,
			}
			start = c.addInst(inst)
		}
		return start
	case NodeTypeAny:
		return c.addInst(Inst{Op: ReOpAny, Out: next})
	case NodeTypeClass:
		class, ok := RuneClassMap[node.Value]
		if !ok {
			panic(fmt.Sprintf("unknown character class: %s", node.Value))
		}
		return c.addInst(Inst{Op: ReOpRuneClass, Arg: class, Out: next})
	case NodeTypeConcat:
		start := next
		for i := len(node.Children) - 1; i >= 0; i-- {
			start = c.compile(node.Children[i], start)
		}
		return start
	case NodeTypeAlternation:
		if len(node.Children) == 1 {
			return c.compile(node.Children[0], next)
		}
		split := c.addInst(Inst{Op: ReOpSplit})
		out := c.compile(node.Children[0], next)
		rest := &ASTNode{Type: NodeTypeAlternation, Children: node.Children[1:]}
		out1 := c.compile(rest, next)
		c.parchSplit(split, out, out1)
		return split
	case NodeTypeStar:
		split := c.addInst(Inst{Op: ReOpSplit})
		body := c.compile(node.Children[0], split)
		c.parchSplit(split, next, body)
		return split
	case NodeTypeQuestion:
		split := c.addInst(Inst{Op: ReOpSplit})
		out := c.compile(node.Children[0], next)
		c.parchSplit(split, out, next)
		return split
	case NodeTypePlus:
		star := c.addInst(Inst{Op: ReOpSplit})
		body := c.compile(node.Children[0], star)
		c.parchSplit(star, body, next)
		split := c.addInst(Inst{Op: ReOpSplit, Out1: next})
		bodyStart := c.compile(node.Children[0], split)
		c.patch(split, bodyStart)
		return bodyStart
	case NodeTypeCapture:
		numCaps := c.nCaps
		c.nCaps++
		end := c.addInst(Inst{Op: ReOpCaptureEnd, Arg: numCaps, Out: next})
		innerStart := c.compile(node.Children[0], end)
		start := c.addInst(Inst{Op: ReOpCaptureStart, Arg: numCaps, Out: innerStart})
		return start
	case NodeTypeStartOfString:
		return c.addInst(Inst{Op: ReOpStartOfString, Out: next})
	case NodeTypeEndOfString:
		return c.addInst(Inst{Op: ReOpEndOfString, Out: next})
	default:
		panic("unknown AST node type")
	}
}
