package compiler

import (
	"fmt"
	"osdrv/liss/ast"
	"osdrv/liss/code"
	"osdrv/liss/lexer"
	"osdrv/liss/object"
	"osdrv/liss/parser"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestComplilerScopes(t *testing.T) {
	c := New()
	assert.Equal(t, 0, c.scopeix, "Initial scope index should be 0")

	c.emit(code.OpTrue)
	c.enterScope()
	assert.Equal(t, 1, c.scopeix, "Scope index should be incremented after entering a scope")

	c.emit(code.OpFalse)

	assert.Len(t, c.scopes[c.scopeix].instrs, 1, "The outer scope should contain exactly one instruction")
	last := c.scopes[c.scopeix].last
	assert.Equal(t, code.OpFalse, last.OpCode, "Last instruction in the outer scope should be an OpFalse")

	c.leaveScope()
	assert.Equal(t, 0, c.scopeix, "Scope index should be decremented after leaving a scope")

	c.emit(code.OpNull)
	assert.Len(t, c.scopes[c.scopeix].instrs, 2, "The inner scope should now contain two instructions")
	assert.Equal(t, code.OpNull, c.scopes[c.scopeix].last.OpCode, "Last instruction in the outer scope should be an OpNull")
	assert.Equal(t, code.OpTrue, c.scopes[c.scopeix].prev.OpCode, "Previous instruction in the outer scope should be an OpTrue")
}

func TestClosures(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantConsts []any
		wantInstrs []code.Instructions
		wantErr    error
	}{
		{
			name: "simple closure",
			input: `
			(fn [a]
				(fn [b] (+ a b))
			)`,
			wantConsts: []any{
				[]code.Instructions{
					code.Make(code.OpGetFree, 0),
					code.Make(code.OpGetLocal, 0),
					code.Make(code.OpAdd, 2),
					code.Make(code.OpReturn),
				},
				[]code.Instructions{
					code.Make(code.OpGetLocal, 0),
					code.Make(code.OpClosure, 0, 1),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 1, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name: "nested closures",
			input: `
			(fn [a]
				(fn [b]
					(fn [c] (+ a b c))
				)
			)`,
			wantConsts: []any{
				[]code.Instructions{
					code.Make(code.OpGetFree, 0),
					code.Make(code.OpGetFree, 1),
					code.Make(code.OpGetLocal, 0),
					code.Make(code.OpAdd, 3),
					code.Make(code.OpReturn),
				},
				[]code.Instructions{
					code.Make(code.OpGetFree, 0),
					code.Make(code.OpGetLocal, 0),
					code.Make(code.OpClosure, 0, 2),
					code.Make(code.OpReturn),
				},
				[]code.Instructions{
					code.Make(code.OpGetLocal, 0),
					code.Make(code.OpClosure, 1, 1),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 2, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name: "closure with let expressions",
			input: `
			(let global 55)
			(fn []
				(let a 66)
				(fn []
					(let b 77)
					(fn []
						(let c 88)
						(+ global a b c)
					)
				)
			)`,
			wantConsts: []any{
				int64(55),
				int64(66),
				int64(77),
				int64(88),
				[]code.Instructions{
					code.Make(code.OpConst, 3),    // c
					code.Make(code.OpSetLocal, 0), // c
					code.Make(code.OpGetLocal, 0), // c
					code.Make(code.OpPop),
					code.Make(code.OpGetGlobal, 0), // global
					code.Make(code.OpGetFree, 0),   // a
					code.Make(code.OpGetFree, 1),   // b
					code.Make(code.OpGetLocal, 0),  // c
					code.Make(code.OpAdd, 4),
					code.Make(code.OpReturn),
				},
				[]code.Instructions{
					code.Make(code.OpConst, 2),    // b
					code.Make(code.OpSetLocal, 0), // b
					code.Make(code.OpGetLocal, 0), // b
					code.Make(code.OpPop),
					code.Make(code.OpGetFree, 0),  // a
					code.Make(code.OpGetLocal, 0), // b
					code.Make(code.OpClosure, 4, 2),
					code.Make(code.OpReturn),
				},
				[]code.Instructions{
					code.Make(code.OpConst, 1),    // a
					code.Make(code.OpSetLocal, 0), // a
					// TODO: optimize away these pops
					code.Make(code.OpGetLocal, 0), // a
					code.Make(code.OpPop),
					code.Make(code.OpGetLocal, 0), // a
					code.Make(code.OpClosure, 5, 1),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),     // global
				code.Make(code.OpSetGlobal, 0), // global
				code.Make(code.OpGetGlobal, 0), // global
				code.Make(code.OpPop),
				code.Make(code.OpClosure, 6, 0),
				code.Make(code.OpPop),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Unexpected error parsing input: %s", tt.input)
			c := New()
			err = c.Compile(prog)
			if tt.wantErr != nil {
				assert.EqualError(t, err, tt.wantErr.Error(), "Expected error does not match")
				return
			}
			assert.NoError(t, err, "Unexpected error compiling program: %s", tt.input)

			bc := c.Bytecode()
			assertInstrs(t, tt.wantInstrs, bc.Instrs)
			assertConsts(t, tt.wantConsts, bc.Consts)
		})
	}
}

func TestCompileStep(t *testing.T) {
	tests := []struct {
		name       string
		input      ast.Node
		managed    bool
		consts     []object.Object
		symbols    *SymbolTable
		wantInstrs []code.Instructions
		wantConsts []any
		wantErr    error
	}{
		{
			name: "unmanaged integer",
			input: &ast.IntegerLiteral{
				Value: 42,
			},
			managed:    false,
			wantConsts: []any{int64(42)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name: "managed integer",
			input: &ast.IntegerLiteral{
				Value: 42,
			},
			managed:    true,
			wantConsts: []any{int64(42)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
			},
		},
		{
			name: "unmanaged boolean",
			input: &ast.BooleanLiteral{
				Value: true,
			},
			managed:    false,
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpPop),
			},
		},
		{
			name: "managed boolean",
			input: &ast.BooleanLiteral{
				Value: false,
			},
			managed:    true,
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpFalse),
			},
		},
		{
			name:       "unmanaged null",
			input:      &ast.NullLiteral{},
			managed:    false,
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpNull),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "managed null",
			input:      &ast.NullLiteral{},
			managed:    true,
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpNull),
			},
		},
		{
			name: "unmanaged float",
			input: &ast.FloatLiteral{
				Value: 3.14,
			},
			managed:    false,
			wantConsts: []any{3.14},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name: "managed float",
			input: &ast.FloatLiteral{
				Value: 3.14,
			},
			managed:    true,
			wantConsts: []any{3.14},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
			},
		},
		{
			name: "unmanaged string",
			input: &ast.StringLiteral{
				Value: "Hello, World!",
			},
			managed:    false,
			wantConsts: []any{"Hello, World!"},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name: "managed string",
			input: &ast.StringLiteral{
				Value: "Hello, World!",
			},
			managed:    true,
			wantConsts: []any{"Hello, World!"},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
			},
		},
		{
			name: "unmanaged + operator + expression",
			input: &ast.OperatorExpr{
				Operator: ast.OperatorPlus,
				Operands: []ast.Node{
					&ast.IntegerLiteral{Value: 1},
					&ast.IntegerLiteral{Value: 2},
				},
			},
			managed: false,
			wantConsts: []any{
				int64(1), int64(2),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpAdd, 2),
				code.Make(code.OpPop),
			},
		},
		{
			name: "managed + operator + expression",
			input: &ast.OperatorExpr{
				Operator: ast.OperatorPlus,
				Operands: []ast.Node{
					&ast.IntegerLiteral{Value: 1},
					&ast.IntegerLiteral{Value: 2},
				},
			},
			managed: true,
			wantConsts: []any{
				int64(1), int64(2),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpAdd, 2),
			},
		},
		{
			name: "unmanaged block expression",
			input: &ast.BlockExpression{
				Nodes: []ast.Node{
					&ast.IntegerLiteral{Value: 1},
					&ast.IntegerLiteral{Value: 2},
					&ast.IntegerLiteral{Value: 3},
				},
			},
			managed: false,
			wantConsts: []any{
				int64(1), int64(2), int64(3),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
				code.Make(code.OpConst, 1),
				code.Make(code.OpPop),
				code.Make(code.OpConst, 2),
				code.Make(code.OpPop),
			},
		},
		{
			name: "managed block expression",
			input: &ast.BlockExpression{
				Nodes: []ast.Node{
					&ast.IntegerLiteral{Value: 1},
					&ast.IntegerLiteral{Value: 2},
					&ast.IntegerLiteral{Value: 3},
				},
			},
			managed: true,
			wantConsts: []any{
				int64(1), int64(2), int64(3),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
				code.Make(code.OpConst, 1),
				code.Make(code.OpPop),
				code.Make(code.OpConst, 2),
			},
		},
		{
			name: "unmanaged cond expr",
			input: &ast.CondExpression{
				Cond: &ast.BooleanLiteral{Value: true},
				Then: &ast.IntegerLiteral{Value: 123},
				Else: &ast.IntegerLiteral{Value: 456},
			},
			managed: false,
			wantConsts: []any{
				int64(123), int64(456),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpJumpIfFalse, 11),
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
				code.Make(code.OpJump, 15),
				code.Make(code.OpConst, 1),
				code.Make(code.OpPop),
			},
		},
		{
			name: "managed cond expr",
			input: &ast.CondExpression{
				Cond: &ast.BooleanLiteral{Value: true},
				Then: &ast.IntegerLiteral{Value: 123},
				Else: &ast.IntegerLiteral{Value: 456},
			},
			managed: true,
			wantConsts: []any{
				int64(123), int64(456),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpJumpIfFalse, 10),
				code.Make(code.OpConst, 0),
				code.Make(code.OpJump, 13),
				code.Make(code.OpConst, 1),
			},
		},
		{
			name: "unmanaged let expr",
			input: &ast.LetExpression{
				Identifier: &ast.IdentifierExpr{Name: "x"},
				Value:      &ast.IntegerLiteral{Value: 42},
			},
			managed: false,
			wantConsts: []any{
				int64(42),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpSetGlobal, 0),
				code.Make(code.OpGetGlobal, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name: "unmanaged let expr",
			input: &ast.LetExpression{
				Identifier: &ast.IdentifierExpr{Name: "x"},
				Value:      &ast.IntegerLiteral{Value: 42},
			},
			managed: true,
			wantConsts: []any{
				int64(42),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpSetGlobal, 0),
				code.Make(code.OpGetGlobal, 0),
			},
		},
		{
			name: "unmanaged cond expr with no else",
			input: &ast.CondExpression{
				Cond: &ast.BooleanLiteral{Value: true},
				Then: &ast.IntegerLiteral{Value: 123},
			},
			managed: false,
			wantConsts: []any{
				int64(123),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpJumpIfFalse, 11),
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
				code.Make(code.OpJump, 13),
				code.Make(code.OpNull),
				code.Make(code.OpPop),
			},
		},
		{
			name: "managed cond expr with no else",
			input: &ast.CondExpression{
				Cond: &ast.BooleanLiteral{Value: true},
				Then: &ast.IntegerLiteral{Value: 123},
			},
			managed: true,
			wantConsts: []any{
				int64(123),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpJumpIfFalse, 10),
				code.Make(code.OpConst, 0),
				code.Make(code.OpJump, 11),
				code.Make(code.OpNull),
			},
		},
		{
			name: "unmanaged cond expr with else",
			input: &ast.CondExpression{
				Cond: &ast.BooleanLiteral{Value: true},
				Then: &ast.IntegerLiteral{Value: 123},
				Else: &ast.IntegerLiteral{Value: 456},
			},
			managed: false,
			wantConsts: []any{
				int64(123), int64(456),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpJumpIfFalse, 11),
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
				code.Make(code.OpJump, 15),
				code.Make(code.OpConst, 1),
				code.Make(code.OpPop),
			},
		},
		{
			name: "managed cond expr with else",
			input: &ast.CondExpression{
				Cond: &ast.BooleanLiteral{Value: true},
				Then: &ast.IntegerLiteral{Value: 123},
				Else: &ast.IntegerLiteral{Value: 456},
			},
			managed: true,
			wantConsts: []any{
				int64(123), int64(456),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpJumpIfFalse, 10),
				code.Make(code.OpConst, 0),
				code.Make(code.OpJump, 13),
				code.Make(code.OpConst, 1),
			},
		},
		{
			name: "unmanaged identifier expression",
			input: &ast.IdentifierExpr{
				Name: "myVar",
			},
			symbols: (func() *SymbolTable {
				s := NewSymbolTable()
				s.Define("myVar")
				return s
			})(),
			managed:    false,
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpGetGlobal, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name: "managed identifier expression",
			input: &ast.IdentifierExpr{
				Name: "myVar",
			},
			symbols: (func() *SymbolTable {
				s := NewSymbolTable()
				s.Define("myVar")
				return s
			})(),
			managed:    true,
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpGetGlobal, 0),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			consts := []object.Object{}
			symbols := NewSymbolTable()
			if tt.consts != nil {
				consts = tt.consts
			}
			if tt.symbols != nil {
				symbols = tt.symbols
			}
			c := NewWithState(symbols, consts)
			err := c.compileStep(tt.input, tt.managed)
			if tt.wantErr != nil {
				assert.EqualError(t, err, tt.wantErr.Error(), "Expected error does not match")
				return
			}
			assert.NoError(t, err, "Unexpected error compiling program: %s", tt.input)

			bc := c.Bytecode()
			assertInstrs(t, tt.wantInstrs, bc.Instrs)
			assertConsts(t, tt.wantConsts, bc.Consts)
		})
	}
}

func TestCompile(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantInstrs []code.Instructions
		wantConsts []any
		wantErr    error
	}{
		{
			name:       "single atom",
			input:      "42",
			wantConsts: []any{int64(42)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "null",
			input:      "null",
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpNull),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "multiple atoms",
			input:      "1 2 3 4 5",
			wantConsts: []any{int64(1), int64(2), int64(3), int64(4), int64(5)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
				code.Make(code.OpConst, 1),
				code.Make(code.OpPop),
				code.Make(code.OpConst, 2),
				code.Make(code.OpPop),
				code.Make(code.OpConst, 3),
				code.Make(code.OpPop),
				code.Make(code.OpConst, 4),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "simple arithmetic",
			input:      "(+ 1 2 3)",
			wantConsts: []any{int64(1), int64(2), int64(3)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpConst, 2),
				code.Make(code.OpAdd, 3),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "emit stack pop",
			input:      "(+ 1 2 5)(+ 3 4)",
			wantConsts: []any{int64(1), int64(2), int64(5), int64(3), int64(4)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpConst, 2),
				code.Make(code.OpAdd, 3),
				code.Make(code.OpPop),
				code.Make(code.OpConst, 3),
				code.Make(code.OpConst, 4),
				code.Make(code.OpAdd, 2),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "multiplication",
			input:      "(* 1 2 3)",
			wantConsts: []any{int64(1), int64(2), int64(3)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpConst, 2),
				code.Make(code.OpMul, 3),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "string multiplication",
			input:      `(* "Hello" 3)`,
			wantConsts: []any{"Hello", int64(3)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpMul, 2),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "division",
			input:      "(/ 6 2)",
			wantConsts: []any{int64(6), int64(2)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpDiv),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "subtraction",
			input:      "(- 5 3)",
			wantConsts: []any{int64(5), int64(3)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpSub),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "boolean atom true",
			input:      "true",
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "boolean atom false",
			input:      "false",
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpFalse),
				code.Make(code.OpPop),
			},
		},
		{
			name:    "wrong argc",
			input:   "(>= 1 2 3 4)",
			wantErr: fmt.Errorf("expected 2 arguments for operator >=, got 4"),
		},
		{
			name:       "expression with an atomic value",
			input:      `(true)`,
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpPop),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Unexpected error parsing input: %s", tt.input)
			c := New()
			err = c.Compile(prog)
			if tt.wantErr != nil {
				assert.EqualError(t, err, tt.wantErr.Error(), "Expected error does not match")
				return
			}
			assert.NoError(t, err, "Unexpected error compiling program: %s", tt.input)

			bc := c.Bytecode()
			assertInstrs(t, tt.wantInstrs, bc.Instrs)
			assertConsts(t, tt.wantConsts, bc.Consts)
		})
	}
}

func TestComparison(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantInstrs []code.Instructions
		wantConsts []any
		wantErr    error
	}{
		{
			name:       "basic comparison: equal",
			input:      "(= 5 5)",
			wantConsts: []any{int64(5), int64(5)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpEql),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "basic comparison: not equal",
			input:      "(!= 5 3)",
			wantConsts: []any{int64(5), int64(3)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpNotEql),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "basic comparison: greater than",
			input:      "(> 5 3)",
			wantConsts: []any{int64(5), int64(3)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpGreaterThan),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "basic comparison: greater than or equal",
			input:      "(>= 5 5)",
			wantConsts: []any{int64(5), int64(5)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpGreaterEqual),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "basic comparison: less than",
			input:      "(< 3 5)",
			wantConsts: []any{int64(3), int64(5)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpLessThan),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "basic comparison: less than or equal",
			input:      "(<= 3 3)",
			wantConsts: []any{int64(3), int64(3)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpLessEqual),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "boolean negation",
			input:      "(not true)",
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpNot),
				code.Make(code.OpPop),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Unexpected error parsing input: %s", tt.input)
			c := New()
			err = c.Compile(prog)
			assert.NoError(t, err, "Unexpected error compiling program: %s", tt.input)

			bc := c.Bytecode()
			assertInstrs(t, tt.wantInstrs, bc.Instrs)
			assertConsts(t, tt.wantConsts, bc.Consts)
		})
	}
}

func TestCond(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantInstrs []code.Instructions
		wantErr    error
	}{
		{
			name:  "cond with then condition",
			input: `(cond true 123)`,
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpJumpIfFalse, 11),
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
				code.Make(code.OpJump, 13),
				code.Make(code.OpNull),
				code.Make(code.OpPop),
			},
		},
		{
			name:  "cond with an else branch",
			input: `(cond true 123 456)`,
			wantInstrs: []code.Instructions{
				code.Make(code.OpTrue),
				code.Make(code.OpJumpIfFalse, 11),
				code.Make(code.OpConst, 0),
				code.Make(code.OpPop),
				code.Make(code.OpJump, 15),
				code.Make(code.OpConst, 1),
				code.Make(code.OpPop),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Unexpected error parsing input: %s", tt.input)
			c := New()
			err = c.Compile(prog)
			if tt.wantErr != nil {
				assert.EqualError(t, err, tt.wantErr.Error(), "Expected error does not match")
				return
			}
			assert.NoError(t, err, "Unexpected error compiling program: %s", tt.input)

			bc := c.Bytecode()
			assertInstrs(t, tt.wantInstrs, bc.Instrs)
		})
	}
}

func TestLetExpr(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantInstrs []code.Instructions
		wantConsts []any
		wantErr    error
	}{
		{
			name:       "let with single binding",
			input:      `(let x 42)`,
			wantConsts: []any{int64(42)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpSetGlobal, 0),
				code.Make(code.OpGetGlobal, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "let with single binding and resolve",
			input:      `(let x 42) x`,
			wantConsts: []any{int64(42)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpSetGlobal, 0),
				code.Make(code.OpGetGlobal, 0),
				code.Make(code.OpPop),
				code.Make(code.OpGetGlobal, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "let with a reference to another let",
			input:      `(let y (let x 42))`,
			wantConsts: []any{int64(42)},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpSetGlobal, 0),
				code.Make(code.OpGetGlobal, 0),
				code.Make(code.OpSetGlobal, 1),
				code.Make(code.OpGetGlobal, 1),
				code.Make(code.OpPop),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Unexpected error parsing input: %s", tt.input)
			c := New()
			err = c.Compile(prog)
			if tt.wantErr != nil {
				assert.EqualError(t, err, tt.wantErr.Error(), "Expected error does not match")
				return
			}
			assert.NoError(t, err, "Unexpected error compiling program: %s", tt.input)

			bc := c.Bytecode()
			assertInstrs(t, tt.wantInstrs, bc.Instrs)
			assertConsts(t, tt.wantConsts, bc.Consts)
		})
	}
}

func TestFunctionExpr(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantInstrs []code.Instructions
		wantConsts []any
		wantErr    error
	}{
		{
			name:  "simple function no args",
			input: `(fn [] (+ 2 3))`,
			wantConsts: []any{
				int64(2),
				int64(3),
				[]code.Instructions{
					code.Make(code.OpConst, 0),
					code.Make(code.OpConst, 1),
					code.Make(code.OpAdd, 2),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 2, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name:  "simple function no args check managed scope",
			input: `(fn [] (1 2 3))`,
			wantConsts: []any{
				int64(1),
				int64(2),
				int64(3),
				[]code.Instructions{
					code.Make(code.OpConst, 0),
					code.Make(code.OpPop),
					code.Make(code.OpConst, 1),
					code.Make(code.OpPop),
					code.Make(code.OpConst, 2),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 3, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name:  "null-function",
			input: `(fn [] null)`,
			wantConsts: []any{
				[]code.Instructions{
					code.Make(code.OpNull),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 0, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name:  "const-function",
			input: `(fn [] 42)`,
			wantConsts: []any{
				int64(42),
				[]code.Instructions{
					code.Make(code.OpConst, 0),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 1, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name: "function with local bindings",
			input: `
			(fn []
			  (let a 2)
			  (let b 3)
			  (* a b)
			)`,
			wantConsts: []any{
				int64(2),
				int64(3),
				[]code.Instructions{
					code.Make(code.OpConst, 0),
					code.Make(code.OpSetLocal, 0),
					code.Make(code.OpGetLocal, 0),
					code.Make(code.OpPop),
					code.Make(code.OpConst, 1),
					code.Make(code.OpSetLocal, 1),
					code.Make(code.OpGetLocal, 1),
					code.Make(code.OpPop),
					code.Make(code.OpGetLocal, 0),
					code.Make(code.OpGetLocal, 1),
					code.Make(code.OpMul, 2),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 2, 0),
				code.Make(code.OpPop),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Unexpected error parsing input: %s", tt.input)
			c := New()
			err = c.Compile(prog)
			if tt.wantErr != nil {
				assert.EqualError(t, err, tt.wantErr.Error(), "Expected error does not match")
				return
			}
			assert.NoError(t, err, "Unexpected error compiling program: %s", tt.input)

			bc := c.Bytecode()
			assertInstrs(t, tt.wantInstrs, bc.Instrs)
			assertConsts(t, tt.wantConsts, bc.Consts)
		})
	}
}

func TestBuiltinFunctionCall(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantInstrs []code.Instructions
		wantConsts []any
		wantErr    error
	}{
		{
			name:       "len function",
			input:      `(len "Hello")`,
			wantConsts: []any{"Hello"},
			wantInstrs: []code.Instructions{
				code.Make(code.OpGetBuiltin, 0),
				code.Make(code.OpConst, 0),
				code.Make(code.OpCall, 1),
				code.Make(code.OpPop),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Unexpected error parsing input: %s", tt.input)
			c := New()
			err = c.Compile(prog)
			if tt.wantErr != nil {
				assert.EqualError(t, err, tt.wantErr.Error(), "Expected error does not match")
				return
			}
			assert.NoError(t, err, "Unexpected error compiling program: %s", tt.input)

			bc := c.Bytecode()
			assertInstrs(t, tt.wantInstrs, bc.Instrs)
			assertConsts(t, tt.wantConsts, bc.Consts)
		})
	}
}

func TestFunctionCall(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantInstrs []code.Instructions
		wantConsts []any
		wantErr    error
	}{
		{
			name:  "simple function call",
			input: `((fn [] (+ 2 3)))`,
			wantConsts: []any{
				int64(2),
				int64(3),
				[]code.Instructions{
					code.Make(code.OpConst, 0),
					code.Make(code.OpConst, 1),
					code.Make(code.OpAdd, 2),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 2, 0),
				code.Make(code.OpCall, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name:  "unnamed function call with arguments",
			input: `((fn [a b c] (+ a b c)) 1 2 3)`,
			wantConsts: []any{
				[]code.Instructions{
					code.Make(code.OpGetLocal, 0),
					code.Make(code.OpGetLocal, 1),
					code.Make(code.OpGetLocal, 2),
					code.Make(code.OpAdd, 3),
					code.Make(code.OpReturn),
				},
				int64(1),
				int64(2),
				int64(3),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 0, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpConst, 2),
				code.Make(code.OpConst, 3),
				code.Make(code.OpCall, 3),
				code.Make(code.OpPop),
			},
		},
		{
			name: "named function call",
			input: `
			(fn foo [] (+ 2 3))
			(foo)`,
			wantConsts: []any{
				int64(2),
				int64(3),
				[]code.Instructions{
					code.Make(code.OpConst, 0),
					code.Make(code.OpConst, 1),
					code.Make(code.OpAdd, 2),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 2, 0),
				code.Make(code.OpSetGlobal, 0),
				code.Make(code.OpPop),
				code.Make(code.OpGetGlobal, 0),
				code.Make(code.OpCall, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name: "named function call with args",
			input: `
			(fn sum_3 [a b c] (+ a b c))
			(sum_3 1 2 3)`,
			wantConsts: []any{
				[]code.Instructions{
					code.Make(code.OpGetLocal, 0),
					code.Make(code.OpGetLocal, 1),
					code.Make(code.OpGetLocal, 2),
					code.Make(code.OpAdd, 3),
					code.Make(code.OpReturn),
				},
				int64(1),
				int64(2),
				int64(3),
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 0, 0),
				code.Make(code.OpSetGlobal, 0),
				code.Make(code.OpPop),
				code.Make(code.OpGetGlobal, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpConst, 2),
				code.Make(code.OpConst, 3),
				code.Make(code.OpCall, 3),
				code.Make(code.OpPop),
			},
		},
		{
			name: "function call chain",
			input: `
			(fn foo [] (+ 2 3))
			(fn bar [] (foo))
			(bar)`,
			wantConsts: []any{
				int64(2),
				int64(3),
				[]code.Instructions{
					code.Make(code.OpConst, 0),
					code.Make(code.OpConst, 1),
					code.Make(code.OpAdd, 2),
					code.Make(code.OpReturn),
				},
				[]code.Instructions{
					code.Make(code.OpGetGlobal, 0),
					code.Make(code.OpCall, 0),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 2, 0),
				code.Make(code.OpSetGlobal, 0),
				code.Make(code.OpPop),
				code.Make(code.OpClosure, 3, 0),
				code.Make(code.OpSetGlobal, 1),
				code.Make(code.OpPop),
				code.Make(code.OpGetGlobal, 1),
				code.Make(code.OpCall, 0),
				code.Make(code.OpPop),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Unexpected error parsing input: %s", tt.input)
			c := New()
			err = c.Compile(prog)
			if tt.wantErr != nil {
				assert.EqualError(t, err, tt.wantErr.Error(), "Expected error does not match")
				return
			}
			assert.NoError(t, err, "Unexpected error compiling program: %s", tt.input)

			bc := c.Bytecode()
			assertInstrs(t, tt.wantInstrs, bc.Instrs)
			assertConsts(t, tt.wantConsts, bc.Consts)
		})
	}
}

func TestLetStatementScopes(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantInstrs []code.Instructions
		wantConsts []any
		wantErr    error
	}{
		{
			name: "global scope",
			input: `
			(let num 42)
			(fn [] num)
			`,
			wantConsts: []any{
				int64(42),
				[]code.Instructions{
					code.Make(code.OpGetGlobal, 0),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpSetGlobal, 0),
				code.Make(code.OpGetGlobal, 0),
				code.Make(code.OpPop),
				code.Make(code.OpClosure, 1, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name: "local scope",
			input: `
			(fn []
			  (let a 2)
			  (let b 3)
			  (* a b)
			)
			`,
			wantConsts: []any{
				int64(2),
				int64(3),
				[]code.Instructions{
					code.Make(code.OpConst, 0),
					code.Make(code.OpSetLocal, 0),
					code.Make(code.OpGetLocal, 0),
					code.Make(code.OpPop),
					code.Make(code.OpConst, 1),
					code.Make(code.OpSetLocal, 1),
					code.Make(code.OpGetLocal, 1),
					code.Make(code.OpPop),
					code.Make(code.OpGetLocal, 0),
					code.Make(code.OpGetLocal, 1),
					code.Make(code.OpMul, 2),
					code.Make(code.OpReturn),
				},
			},
			wantInstrs: []code.Instructions{
				code.Make(code.OpClosure, 2, 0),
				code.Make(code.OpPop),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Unexpected error parsing input: %s", tt.input)
			c := New()
			err = c.Compile(prog)
			if tt.wantErr != nil {
				assert.EqualError(t, err, tt.wantErr.Error(), "Expected error does not match")
				return
			}
			assert.NoError(t, err, "Unexpected error compiling program: %s", tt.input)

			bc := c.Bytecode()
			assertInstrs(t, tt.wantInstrs, bc.Instrs)
			assertConsts(t, tt.wantConsts, bc.Consts)
		})
	}
}

func TestListExpr(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantInstrs []code.Instructions
		wantConsts []any
		wantErr    error
	}{
		{
			name:       "empty list",
			input:      `[]`,
			wantConsts: []any{},
			wantInstrs: []code.Instructions{
				code.Make(code.OpList, 0),
				code.Make(code.OpPop),
			},
		},
		{
			name:       "list with elements",
			input:      `[1 "foo" true]`,
			wantConsts: []any{int64(1), "foo"},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpTrue),
				code.Make(code.OpList, 3),
				code.Make(code.OpPop),
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Unexpected error parsing input: %s", tt.input)
			c := New()
			err = c.Compile(prog)
			if tt.wantErr != nil {
				assert.EqualError(t, err, tt.wantErr.Error(), "Expected error does not match")
				return
			}
			assert.NoError(t, err, "Unexpected error compiling program: %s", tt.input)

			bc := c.Bytecode()
			assertInstrs(t, tt.wantInstrs, bc.Instrs)
			assertConsts(t, tt.wantConsts, bc.Consts)
		})
	}
}

func assertInstrs(t *testing.T, wants []code.Instructions, got code.Instructions) {
	want := concatArr(wants)
	wantStr := code.PrintInstr(want)
	gotStr := code.PrintInstr(got)
	assert.Equal(t, want, got,
		fmt.Sprintf("Expected instructions do not match:\nwant:\n%s\n\ngot:\n%s",
			wantStr, gotStr))
}

func assertConsts(t *testing.T, want []any, got []object.Object) {
	assert.Len(t, got, len(want), "Expected number of constants do not match")
	for i, w := range want {
		if i >= len(got) {
			t.Errorf("Index %d out of bounds for constants", i)
			continue
		}
		switch v := got[i].(type) {
		case *object.Integer:
			assert.Equal(t, w, v.Raw(), "Expected constant at index %d to match", i)
		case *object.Float:
			assert.Equal(t, w, v.Raw(), "Expected constant at index %d to match", i)
		case *object.String:
			assert.Equal(t, w, v.Raw(), "Expected constant at index %d to match", i)
		case *object.Function:
			winstr := w.([]code.Instructions)
			assertInstrs(t, winstr, v.Instrs)
		default:
			t.Errorf("Unexpected constant type at index %d: %T", i, got[i])
		}
	}
}

func parse(source string) (ast.Node, error) {
	lex := lexer.NewLexer(source)
	par := parser.NewParser(lex)
	return par.Parse()
}

func concatArr[T any](arrs [][]T) []T {
	res := make([]T, 0, 1)
	for _, arr := range arrs {
		res = append(res, arr...)
	}
	return res
}
