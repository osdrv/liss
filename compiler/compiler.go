package compiler

import (
	"fmt"
	"osdrv/liss/ast"
	"osdrv/liss/code"
	"osdrv/liss/object"
)

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

	ast.OperatorBitwiseAnd:        2,
	ast.OperatorBitwiseOr:         2,
	ast.OperatorBitwiseXor:        2,
	ast.OperatorBitwiseNot:        1,
	ast.OperatorBitwiseShiftLeft:  2,
	ast.OperatorBitwiseShiftRight: 2,
}

type CompilerOptions struct {
	Debug bool
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
	symbols *object.SymbolTable

	scopes  []CompilationScope
	scopeix int

	opts     CompilerOptions
	prevLine int
}

func New(opts CompilerOptions) *Compiler {
	s0 := CompilationScope{
		instrs: make(code.Instructions, 0),
		last:   EmittedInstruction{},
		prev:   EmittedInstruction{},
	}

	return &Compiler{
		consts:   make([]object.Object, 0),
		symbols:  object.NewSymbolTable(),
		scopes:   []CompilationScope{s0},
		scopeix:  0,
		prevLine: -1,
		opts:     opts,
	}
}

func NewWithState(opts CompilerOptions, symbols *object.SymbolTable, consts []object.Object) *Compiler {
	c := New(opts)
	c.symbols = symbols
	c.consts = consts
	return c
}

func (c *Compiler) compileStep(node ast.Node, managed bool, isTail bool) error {
	if c.opts.Debug {
		tok := node.Token()
		if c.prevLine != tok.Location.Line {
			c.emit(code.OpSrcAnchor, tok.Location.Line)
			c.prevLine = tok.Location.Line
		}
	}
	switch n := node.(type) {
	case *ast.BlockExpression:
		for ix, expr := range n.Nodes {
			// only the last expression of the block can be in a tail position
			isLast := ix == len(n.Nodes)-1
			m := isLast && managed
			if err := c.compileStep(expr, m, isLast && isTail); err != nil {
				return err
			}
		}
		if !managed && len(n.Nodes) > 0 {
			if !c.lastInstrIs(code.OpPop) {
				c.emit(code.OpPop)
			}
		}
	case *ast.OperatorExpr:
		argc := len(n.Operands)
		if nop, ok := AssertOperatorArgc[n.Operator]; ok {
			if argc != nop {
				return fmt.Errorf("expected %d arguments for operator %s, got %d", nop, n.Operator, argc)
			}
		}
		for i := range len(n.Operands) {
			if err := c.compileStep(n.Operands[i], true, false); err != nil {
				return err
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
		case ast.OperatorAnd:
			c.emit(code.OpAnd, argc)
		case ast.OperatorOr:
			c.emit(code.OpOr, argc)
		case ast.OperatorModulus:
			c.emit(code.OpMod)
		case ast.OperatorBitwiseAnd:
			c.emit(code.OpBAnd)
		case ast.OperatorBitwiseOr:
			c.emit(code.OpBOr)
		case ast.OperatorBitwiseXor:
			c.emit(code.OpBXor)
		case ast.OperatorBitwiseNot:
			c.emit(code.OpBNot)
		case ast.OperatorBitwiseShiftLeft:
			c.emit(code.OpBShiftLeft)
		case ast.OperatorBitwiseShiftRight:
			c.emit(code.OpBShiftRight)
		default:
			return fmt.Errorf("unsupported operator: %s", n.Operator)
		}
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.CondExpression:
		if err := c.compileStep(n.Cond, true, false); err != nil {
			return err
		}
		jmp1 := c.emit(code.OpJumpIfFalse, 9999)
		if err := c.compileStep(n.Then, managed, isTail); err != nil {
			return err
		}
		jmp2 := c.emit(code.OpJump, 9999)
		jmpTo := len(c.currentInstrs())
		c.updOperand(jmp1, jmpTo)
		if n.Else != nil {
			if err := c.compileStep(n.Else, managed, isTail); err != nil {
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
	case *ast.SwitchExpression:
		// This implementation treats a switch expression as a series of if-else-if expressions.
		// The main expression is re-evaluated for each case, which is a limitation
		// due to the lack of an OpDup instruction in the VM.
		endJumps := []int{}
		var nextCaseJumpPos int

		for _, caseExpr := range n.Cases {
			if nextCaseJumpPos != 0 {
				c.updOperand(nextCaseJumpPos, len(c.currentInstrs()))
			}

			// Condition: n.Expr == caseExpr.When
			if err := c.compileStep(n.Expr, true, false); err != nil {
				return err
			}
			if err := c.compileStep(caseExpr.When, true, false); err != nil {
				return err
			}
			c.emit(code.OpEql)

			nextCaseJumpPos = c.emit(code.OpJumpIfFalse, 9999)

			// Then block
			if err := c.compileStep(caseExpr.Then, managed, isTail); err != nil {
				return err
			}
			endJumps = append(endJumps, c.emit(code.OpJump, 9999))
		}

		if nextCaseJumpPos != 0 {
			c.updOperand(nextCaseJumpPos, len(c.currentInstrs()))
		}

		// Default case
		if n.Default != nil {
			if err := c.compileStep(n.Default.Then, managed, isTail); err != nil {
				return err
			}
		} else {
			c.emit(code.OpNull)
			if !managed {
				c.emit(code.OpPop)
			}
		}

		for _, jmp := range endJumps {
			c.updOperand(jmp, len(c.currentInstrs()))
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
	case *ast.ListExpression:
		for _, item := range n.Items {
			if err := c.compileStep(item, true, false); err != nil {
				return err
			}
		}
		c.emit(code.OpList, len(n.Items))
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.LetExpression:
		if n.Identifier.HasModule() {
			return fmt.Errorf("cannot define variable with module prefix: %s", n.Identifier.FullName())
		}
		sym, err := c.symbols.Define(object.MODULE_SELF, n.Identifier.FullName())
		if err != nil {
			return err
		}
		if err := c.compileStep(n.Value, true, false); err != nil {
			return err
		}

		if sym.Scope == object.GlobalScope {
			c.emit(code.OpSetGlobal, sym.Index)
			c.emit(code.OpGetGlobal, sym.Index)
		} else {
			c.emit(code.OpSetLocal, sym.Index)
			c.emit(code.OpGetLocal, sym.Index)
		}
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.IdentifierExpr:
		sym, ok := c.symbols.Resolve(n.Module, n.Name)
		if !ok {
			return fmt.Errorf("undefined variable: %s", n.FullName())
		}
		if err := c.loadSymbol(sym); err != nil {
			return err
		}
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.FunctionExpression:
		var fnsym *object.Symbol
		var name string
		if n.Name != nil {
			name = n.Name.Name
			if len(name) > 0 {
				sym, err := c.symbols.Define(object.MODULE_SELF, name)
				if err != nil {
					return err
				}
				fnsym = &sym
			}
		}

		c.enterScope()

		if n.Name != nil {
			if len(name) > 0 {
				c.symbols.DefineFunctionName(n.Name.Name)
			}
		}

		for _, arg := range n.Args {
			c.symbols.Define(object.MODULE_SELF, arg.Name)
		}

		err := c.compileStep(ast.NewBlockExpression(node.Token(), n.Body), true, true)
		if err != nil {
			return err
		}
		if c.lastInstrIs(code.OpPop) {
			c.removeLastInstr()
		}
		c.emit(code.OpReturn)

		free := c.symbols.Free
		numLocals := c.symbols.NumVars
		instrs := c.leaveScope()

		for _, f := range free {
			c.loadSymbol(f)
		}

		args := make([]string, 0, len(n.Args))
		for _, arg := range n.Args {
			args = append(args, arg.Name)
		}
		fn := object.NewFunction(name, args, instrs)
		fn.NumLocals = numLocals
		c.emit(code.OpClosure, c.addConst(fn), len(free))

		if fnsym != nil {
			if fnsym.Scope == object.GlobalScope {
				c.emit(code.OpSetGlobal, fnsym.Index)
				c.emit(code.OpGetGlobal, fnsym.Index)
			} else {
				c.emit(code.OpSetLocal, fnsym.Index)
				c.emit(code.OpGetLocal, fnsym.Index)
			}
		}
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.CallExpression:
		callee := n.Callee
		if calleeIdent, ok := callee.(*ast.IdentifierExpr); ok {
			sym, ok := c.symbols.Resolve(calleeIdent.Module, calleeIdent.Name)
			if !ok {
				return fmt.Errorf("undefined function: %s:%s",
					calleeIdent.Module,
					calleeIdent.Name)
			}
			switch sym.Scope {
			case object.GlobalScope:
				c.emit(code.OpGetGlobal, sym.Index)
			case object.LocalScope:
				c.emit(code.OpGetLocal, sym.Index)
			case object.BuiltinScope:
				c.emit(code.OpGetBuiltin, sym.Index)
			case object.FreeScope:
				c.emit(code.OpGetFree, sym.Index)
			case object.FunctionScope:
				c.emit(code.OpCurrentClosure)
			case object.ModuleScope:
				c.emit(code.OpGetModule, sym.ModIndex, sym.Index)
			default:
				return fmt.Errorf("unsupported symbol scope: %v", sym.Scope)
			}
			for _, arg := range n.Args {
				if err := c.compileStep(arg, true, false); err != nil {
					return err
				}
			}
		} else if calleeFn, ok := callee.(*ast.FunctionExpression); ok {
			if err := c.compileStep(calleeFn, true, false); err != nil {
				return err
			}
			for _, arg := range n.Args {
				if err := c.compileStep(arg, true, false); err != nil {
					return err
				}
			}
		} else {
			return fmt.Errorf("unsupported callee type: %T", callee)
		}

		if isTail {
			c.emit(code.OpTailCall, len(n.Args))
		} else {
			c.emit(code.OpCall, len(n.Args))
		}
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.BreakpointExpression:
		c.emit(code.OpBreakpoint,
			node.Token().Location.Line, node.Token().Location.Column)
	case *ast.RaiseExpression:
		if err := c.compileStep(n.Expr, true, false); err != nil {
			return err
		}
		c.emit(code.OpRaise)
	case *ast.TryExpression:
		loc1 := c.emit(code.OpTryBegin, 9999)
		if err := c.compileStep(n.Body, true, false); err != nil {
			return err
		}
		c.emit(code.OpTryEnd)
		c.updOperand(loc1, len(c.currentInstrs()))
		if !managed {
			c.emit(code.OpPop)
		}
	case *ast.ImportExpression:
		ref := n.Ref.(*ast.StringLiteral).Value
		modix, ok := c.symbols.LookupModule(ref)
		if !ok {
			if n.Alias != nil {
				alias := n.Alias.Name
				modix, ok = c.symbols.LookupModule(alias)
			}
		}
		if !ok {
			return fmt.Errorf("module could not be found: %s", ref)
		}
		c.emit(code.OpLoadModule, modix)
	default:
		return fmt.Errorf("unsupported node type: %T", node)
	}

	return nil
}

func (c *Compiler) loadSymbol(sym object.Symbol) error {
	switch sym.Scope {
	case object.GlobalScope:
		c.emit(code.OpGetGlobal, sym.Index)
	case object.LocalScope:
		c.emit(code.OpGetLocal, sym.Index)
	case object.BuiltinScope:
		c.emit(code.OpGetBuiltin, sym.Index)
	case object.FreeScope:
		c.emit(code.OpGetFree, sym.Index)
	case object.FunctionScope:
		c.emit(code.OpCurrentClosure)
	case object.ModuleScope:
		c.emit(code.OpGetModule, sym.ModIndex, sym.Index)
	default:
		return fmt.Errorf("unsupported symbol scope: %v", sym.Scope)
	}
	return nil
}

func (c *Compiler) Compile(prog ast.Node) error {
	if err := c.compileStep(prog, false, false); err != nil {
		return fmt.Errorf("Compilation error at line: %d: %s", c.prevLine+1, err.Error())
	}
	return nil
}

func (c *Compiler) Bytecode() *Bytecode {
	return &Bytecode{
		Instrs: c.currentInstrs(),
		Consts: c.consts,
	}
}

func (c *Compiler) Symbols() *object.SymbolTable {
	return c.symbols
}

func (c *Compiler) ImportModule(ref string, mod *Module, symbols []string) (*object.Module, error) {
	objmod := object.NewModule(mod.Name, mod.Path, mod.Bytecode.Instrs,
		mod.Symbols, mod.Env)

	modix := c.addConst(objmod)
	if err := c.symbols.DefineModule(ref, objmod, modix); err != nil {
		return nil, err
	}

	for _, sym := range mod.Symbols.Export(symbols) {
		c.Symbols().Define(objmod.Name, sym.Name)
	}

	return objmod, nil
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
	c.symbols = object.NewNestedSymbolTable(c.symbols)
	c.scopeix++
}

func (c *Compiler) leaveScope() code.Instructions {
	instrs := c.currentInstrs()
	c.scopes = c.scopes[:len(c.scopes)-1]
	c.scopeix--
	c.symbols = c.symbols.Outer
	return instrs
}

func (c *Compiler) lastInstrIs(op code.OpCode) bool {
	instrs := c.currentInstrs()
	if len(instrs) == 0 {
		return false
	}
	return code.OpCode(instrs[len(instrs)-1]) == op
}

func (c *Compiler) removeLastInstr() {
	instrs := c.currentInstrs()
	if len(instrs) == 0 {
		return
	}
	instrs = instrs[:len(instrs)-1]
	c.scopes[c.scopeix].instrs = instrs
	c.scopes[c.scopeix].last = c.scopes[c.scopeix].prev
}
