package compiler

import (
	"osdrv/liss/ast"
	"osdrv/liss/code"
	"osdrv/liss/lexer"
	"osdrv/liss/object"
	"osdrv/liss/parser"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestCompile(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantInstrs []code.Instructions
		wantConsts []any
		wantErr    error
	}{
		{
			name:       "simple arithmetic",
			input:      "(+ 1 2 3)",
			wantConsts: []any{1, 2, 3},
			wantInstrs: []code.Instructions{
				code.Make(code.OpConst, 0),
				code.Make(code.OpConst, 1),
				code.Make(code.OpConst, 2),
				code.Make(code.OpAdd, 3),
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

func assertInstrs(t *testing.T, want []code.Instructions, got code.Instructions) {
	assert.Equal(t, concatArr(want), got, "Expected instructions do not match")
}

func assertConsts(t *testing.T, want []any, got []object.Object) {
	assert.Len(t, got, len(want), "Expected number of constants do not match")
	for i, w := range want {
		switch v := w.(type) {
		case int64:
			assert.Equal(t, got[i].(*object.Integer).Value, v, "Expected constant at index %d to match", i)
		}
	}
}

func parse(source string) (*ast.Program, error) {
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
