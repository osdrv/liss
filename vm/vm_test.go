package vm

import (
	"osdrv/liss/ast"
	"osdrv/liss/compiler"
	"osdrv/liss/lexer"
	"osdrv/liss/object"
	"osdrv/liss/parser"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestRun(t *testing.T) {
	tests := []struct {
		name    string
		input   string
		want    any
		wantErr error
	}{
		{
			name:  "Integer literal",
			input: "42",
			want:  int64(42),
		},
		{
			name:  "Negative integer literal",
			input: "-42",
			want:  int64(-42),
		},
		{
			name:  "basic arithmetic",
			input: "(+ 1 2 3)",
			want:  int64(6),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Failed to parse input: %s", tt.input)
			comp := compiler.New()
			assert.NoError(t, comp.Compile(prog), "Failed to compile program: %s", tt.input)
			vm := New(comp.Bytecode())
			assert.NoError(t, vm.Run(), "Failed to run program: %s", tt.input)
			st := vm.StackTop()
			assertObjectEql(t, st, tt.want)
		})
	}
}

func assertObjectEql(t *testing.T, got object.Object, want any) {
	switch obj := got.(type) {
	case *object.Integer:
		assert.Equal(t, want, obj.Value, "Expected integer value %v, got %v", want, obj.Value)
	default:
		t.Logf("Unimplemented object type: %d", obj.Type())
	}
}

func parse(source string) (*ast.Program, error) {
	lex := lexer.NewLexer(source)
	par := parser.NewParser(lex)
	return par.Parse()
}
