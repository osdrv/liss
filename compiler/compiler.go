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

type Bytecode struct {
	Instrs code.Instructions
	Consts []object.Object
}

type EmittedInstruction struct {
	OpCode   code.OpCode
	Position int
}

type CompilationScope struct {
	instrs code.Instructions
	last   EmittedInstruction
	prev   EmittedInstruction
}

type Compiler struct {
	consts  []object.Object
	symbols *SymbolTable

	scopes  []CompilationScope
	scopeix int
}

func New() *Compiler {
	s0 := CompilationScope{
		instrs: make(code.Instructions, 0),
		last:   EmittedInstruction{},
		prev:   EmittedInstruction{},
	}

	return &Compiler{
		consts:  make([]object.Object, 0),
		symbols: NewSymbolTable(),
		scopes:  []CompilationScope{s0},
		scopeix: 0,
	}
}

func NewWithState(symbols *SymbolTable, consts []object.Object) *Compiler {
	c := New()
	c.symbols = symbols
	c.consts = consts
	return c
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
	case *ast.NullLiteral:
		c.emit(code.OpNull)
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.CondExpression:
		if err := c.compileStep(n.Cond, NOARGC, true); err != nil {
			return err
		}
		jmp1 := c.emit(code.OpJumpIfFalse, 9999)
		if err := c.compileStep(n.Then, NOARGC, managed); err != nil {
			return err
		}
		jmp2 := c.emit(code.OpJump, 9999)
		jmpTo := len(c.currentInstrs())
		c.updOperand(jmp1, jmpTo)
		if n.Else != nil {
			if err := c.compileStep(n.Else, NOARGC, managed); err != nil {
				return err
			}
		} else {
			c.emit(code.OpNull)
			if !managed {
				c.emit(code.OpPop)
			}
		}
		jmpTo = len(c.currentInstrs())
		c.updOperand(jmp2, jmpTo)
	case *ast.LetExpression:
		if err := c.compileStep(n.Value, NOARGC, true); err != nil {
			return err
		}
		sym, err := c.symbols.Define(n.Identifier.Name)
		if err != nil {
			return err
		}
		c.emit(code.OpSetGlobal, sym.Index)
		c.emit(code.OpGetGlobal, sym.Index)
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.IdentifierExpr:
		sym, ok := c.symbols.Resolve(n.Name)
		if !ok {
			return fmt.Errorf("undefined variable: %s", n.Name)
		}
		c.emit(code.OpGetGlobal, sym.Index)
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.FunctionExpression:
		c.enterScope()
		// We compile the function body and we mark it as a managed scope.
		err := c.compileStep(ast.NewProgram(n.Body), NOARGC, true)
		if err != nil {
			return err
		}
		// Liss functions always return a value, so we emit a return instruction.
		c.emit(code.OpReturn)
		instrs := c.leaveScope()
		var name string
		if n.Name != nil {
			name = n.Name.Name
		}
		args := make([]string, 0, len(n.Args))
		for _, arg := range n.Args {
			args = append(args, arg.Name)
		}
		fn := object.NewFunction(name, args, instrs)
		c.emit(code.OpConst, c.addConst(fn))
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
		Instrs: c.currentInstrs(),
		Consts: c.consts,
	}
}

func (c *Compiler) currentInstrs() code.Instructions {
	return c.scopes[c.scopeix].instrs
}

func (c *Compiler) addInstr(i []byte) int {
	pos := len(c.currentInstrs())
	upd := append(c.currentInstrs(), i...)
	c.scopes[c.scopeix].instrs = upd
	return pos
}

func (c *Compiler) setLastInstr(op code.OpCode, pos int) {
	prev := c.scopes[c.scopeix].last
	last := EmittedInstruction{OpCode: op, Position: pos}
	c.scopes[c.scopeix].prev = prev
	c.scopes[c.scopeix].last = last
}

func (c *Compiler) addConst(obj object.Object) int {
	c.consts = append(c.consts, obj)
	return len(c.consts) - 1
}

func (c *Compiler) emit(op code.OpCode, operands ...int) int {
	instr := code.Make(op, operands...)
	off := c.addInstr(instr)
	c.setLastInstr(op, off)
	return off
}

func (c *Compiler) replaceInstr(pos int, newinstr []byte) {
	instrs := c.currentInstrs()
	for i := range len(newinstr) {
		instrs[pos+i] = newinstr[i]
	}
}

func (c *Compiler) updOperand(pos int, operand int) {
	op := code.OpCode(c.currentInstrs()[pos])
	newinstrs := code.Make(op, operand)
	c.replaceInstr(pos, newinstrs)
}

func (c *Compiler) enterScope() {
	scope := CompilationScope{
		instrs: make(code.Instructions, 0),
		last:   EmittedInstruction{},
		prev:   EmittedInstruction{},
	}
	c.scopes = append(c.scopes, scope)
	c.scopeix++
}

func (c *Compiler) leaveScope() code.Instructions {
	instrs := c.currentInstrs()
	c.scopes = c.scopes[:len(c.scopes)-1]
	c.scopeix--
	return instrs
}
