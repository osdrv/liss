package compiler

import (
	"fmt"
	"osdrv/liss/ast"
	"osdrv/liss/code"
	"osdrv/liss/object"
	"reflect"
)

const NOARGC = -1 // No argument count, used for expressions without operands

var ManagerTypes = map[reflect.Type]bool{
	reflect.TypeOf(&ast.OperatorExpr{}):       true,
	reflect.TypeOf(&ast.LetExpression{}):      true,
	reflect.TypeOf(&ast.FunctionExpression{}): true,
}

var AssertOperatorArgc = map[ast.Operator]int{
	ast.OperatorMinus:              2,
	ast.OperatorDivide:             2,
	ast.OperatorEqual:              2,
	ast.OperatorNotEqual:           2,
	ast.OperatorGreaterThan:        2,
	ast.OperatorGreaterThanOrEqual: 2,
	ast.OperatorLessThan:           2,
	ast.OperatorLessThanOrEqual:    2,
	ast.OperatorNot:                1,
}

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

func (c *Compiler) compileStep(node ast.Node, argc int, managed bool) error {
	switch n := node.(type) {
	case *ast.Program:
		for _, node := range n.Nodes {
			if err := c.compileStep(node, NOARGC, managed); err != nil {
				return err
			}
		}
	case *ast.Expression:
		// pop the first operand (the operator)
		// compute the size of the remaining operands (args)
		// emit the instructions for the remaining operands
		// emit the instruction for the operator
		// emit the length of the args
		if len(n.Operands) == 0 {
			return nil // No operands, nothing to compile
		}
		if _, ok := ManagerTypes[reflect.TypeOf(n.Operands[0])]; ok {
			// The first operand is an owner type. Compile it last. It will manage
			// the stack.
			for i := 1; i < len(n.Operands); i++ {
				// The first operand is responsible for managing the stack.
				if err := c.compileStep(n.Operands[i], NOARGC, true); err != nil {
					return err
				}
			}
			argc := len(n.Operands) - 1
			if err := c.compileStep(n.Operands[0], argc, managed); err != nil {
				return err
			}
			if !managed {
				c.emit(code.OpPop)
			}
		} else {
			for i := range len(n.Operands) {
				if err := c.compileStep(n.Operands[i], NOARGC, managed); err != nil {
					return err
				}
				c.emit(code.OpPop)
			}
		}
	case *ast.OperatorExpr:
		if n, ok := AssertOperatorArgc[n.Operator]; ok {
			if argc != n {
				return fmt.Errorf("expected %d arguments for operator %s, got %d", n, node.String(), argc)
			}
		}
		switch n.Operator {
		case ast.OperatorPlus:
			c.emit(code.OpAdd, argc)
		case ast.OperatorMinus:
			c.emit(code.OpSub)
		case ast.OperatorMultiply:
			c.emit(code.OpMul, argc)
		case ast.OperatorDivide:
			c.emit(code.OpDiv)
		case ast.OperatorEqual:
			c.emit(code.OpEql)
		case ast.OperatorNotEqual:
			c.emit(code.OpNotEql)
		case ast.OperatorGreaterThan:
			c.emit(code.OpGreaterThan)
		case ast.OperatorGreaterThanOrEqual:
			c.emit(code.OpGreaterEqual)
		case ast.OperatorLessThan:
			c.emit(code.OpLessThan)
		case ast.OperatorLessThanOrEqual:
			c.emit(code.OpLessEqual)
		case ast.OperatorNot:
			c.emit(code.OpNot)
		}
	case *ast.IntegerLiteral:
		integer := object.NewInteger(n.Value)
		c.emit(code.OpConst, c.addConst(integer))
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.FloatLiteral:
		float := object.NewFloat(n.Value)
		c.emit(code.OpConst, c.addConst(float))
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.StringLiteral:
		str := object.NewString(n.Value)
		c.emit(code.OpConst, c.addConst(str))
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.BooleanLiteral:
		if n.Value {
			c.emit(code.OpTrue)
		} else {
			c.emit(code.OpFalse)
		}
		if !managed {
			c.emit(code.OpPop)
		}
	}

	return nil
}

func (c *Compiler) Compile(prog *ast.Program) error {
	if err := c.compileStep(prog, NOARGC, false); err != nil {
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
