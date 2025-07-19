package compiler

import (
	"osdrv/liss/ast"
	"osdrv/liss/code"
	"osdrv/liss/object"
)

const NOARGC = -1 // No argument count, used for expressions without operands

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

func (c *Compiler) compileStep(node ast.Node, argc int) error {
	switch n := node.(type) {
	case *ast.Program:
		for _, expr := range n.Exprs {
			if err := c.compileStep(expr, NOARGC); err != nil {
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
			if err := c.compileStep(n.Operands[i], NOARGC); err != nil {
				return err
			}
		}

		argc := len(n.Operands) - 1
		if err := c.compileStep(n.Operands[0], argc); err != nil {
			return err
		}
		c.emit(code.OpPop)

	case *ast.OperatorExpr:
		switch n.Operator {
		case ast.OperatorPlus:
			c.emit(code.OpAdd, argc)
		}
	case *ast.IntegerLiteral:
		integer := object.NewInteger(n.Value)
		c.emit(code.OpConst, c.addConst(integer))
	}

	return nil
}

func (c *Compiler) Compile(prog *ast.Program) error {
	if err := c.compileStep(prog, NOARGC); err != nil {
		return err
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
