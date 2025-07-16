package compiler

import (
	"osdrv/liss/ast"
	"osdrv/liss/code"
	"osdrv/liss/object"
)

type Compiler struct {
	instrs code.Instructions
	consts []object.Object
}

func New() *Compiler {
	return &Compiler{
		instrs: make(code.Instructions, 0),
		consts: make([]object.Object, 0),
	}
}

func (c *Compiler) Compile(node ast.Node) error {
	switch n := node.(type) {
	case *ast.Program:
		for _, nn := range n.Nodes {
			if err := c.Compile(nn); err != nil {
				return err
			}
		}
	case *ast.Expression:
		// pop the first operand (the operator)
		// compute the size of the remaining operands (args)
		// emit the instructions for the remaining operands
		// emit the instruction for the operator
		// emit the length of the args
		for i := 1; i < len(n.Operands); i++ {
			if err := c.Compile(n.Operands[i]); err != nil {
				return err
			}
		}

		argc := len(n.Operands) - 1

		switch op := n.Operands[0].(type) {
		case *ast.OperatorExpr:
			switch op.Operator {
			case ast.OperatorPlus:
				c.emit(code.OpAdd, argc)
			}
		}
	case *ast.IntegerLiteral:
		integer := object.NewInteger(n.Value)
		c.emit(code.OpConst, c.addConst(integer))
	}

	return nil
}

func (c *Compiler) Bytecode() *Bytecode {
	return &Bytecode{
		Instrs: c.instrs,
		Consts: c.consts,
	}
}

func (c *Compiler) addConst(obj object.Object) int {
	c.consts = append(c.consts, obj)
	return len(c.consts) - 1
}

func (c *Compiler) addInstr(instr code.Instructions) int {
	off := len(c.instrs)
	c.instrs = append(c.instrs, instr...)
	return off
}

func (c *Compiler) emit(op code.OpCode, operands ...int) int {
	instr := code.Make(op, operands...)
	off := c.addInstr(instr)
	return off
}

type Bytecode struct {
	Instrs code.Instructions
	Consts []object.Object
}
