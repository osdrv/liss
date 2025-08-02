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
			name:  "expression with an integer literal",
			input: "(1)",
			want:  int64(1),
		},
		{
			name:  "Negative integer literal",
			input: "-42",
			want:  int64(-42),
		},
		{
			name:  "basic arithmetic: integer addition",
			input: "(+ 1 2 3)",
			want:  int64(6),
		},
		{
			name:  "basic arithmetics: float addition",
			input: "(+ 1.5 2.5)",
			want:  float64(4.0),
		},
		{
			name:  "string concatenation",
			input: `(+ "Hello" ", " "world!")`,
			want:  "Hello, world!",
		},
		{
			name:  "string repetition",
			input: `(* 3 "Hello")`,
			want:  "HelloHelloHello",
		},
		{
			name:  "basic arithmetic: integer subtraction",
			input: "(- 5 2)",
			want:  int64(3),
		},
		{
			name:  "basic arithmetic: integer multiplication",
			input: "(* 2 3 4)",
			want:  int64(24),
		},
		{
			name:  "basic arithmetic: integer division",
			input: "(/ 8 2)",
			want:  int64(4),
		},
		{
			name:  "multiple literals",
			input: "1 2 3 4 5",
			want:  int64(5), // Last value should be returned
		},
		{
			name:  "nested expressions",
			input: "(+ (* 2 3) (- 5 1))",
			want:  int64(10), // (6 + 4)
		},
		{
			name:  "boolean true",
			input: "true",
			want:  true,
		},
		{
			name:  "boolean false",
			input: "false",
			want:  false,
		},
		{
			name:  "comparison: equal integers",
			input: "(= 42 42)",
			want:  true,
		},
		{
			name:  "comparison: equal floats",
			input: "(= 3.14 3.14)",
			want:  true,
		},
		{
			name:  "comparison: equal strings",
			input: `(= "hello" "hello")`,
			want:  true,
		},
		{
			name:  "comparison: equal bools",
			input: "(= true true)",
			want:  true,
		},
		{
			name:  "comparison with type casting: int + float",
			input: "(= 42 42.0)",
			want:  true,
		},
		{
			name:  "comparison null vs int",
			input: "(= null 42)",
			want:  false,
		},
		{
			name:  "comparison null vs string",
			input: `(= null "hello")`,
			want:  false,
		},
		{
			name:  "comparison null vs bool",
			input: "(= null true)",
			want:  false,
		},
		{
			name:  "comparison null vs null",
			input: "(= null null)",
			want:  true,
		},
		{
			name:    "comparison with incompatible types",
			input:   "(= 42 true)",
			wantErr: NewTypeMismatchError("Integer Vs Bool"),
		},
		{
			name:  "comparison: unequal ints",
			input: "(!= 42 43)",
			want:  true,
		},
		{
			name:  "comparison: unequal floats",
			input: "(!= 3.14 2.71)",
			want:  true,
		},
		{
			name:  "comparison: unequal strings",
			input: `(!= "hello" "world")`,
			want:  true,
		},
		{
			name:  "comparison: unequal bools",
			input: "(!= true false)",
			want:  true,
		},
		{
			name:  "comparison: unequal with type casting: int + float",
			input: "(!= 42 42.0)",
			want:  false,
		},
		{
			name:    "comparison with incompatible types: int + bool",
			input:   "(!= 42 true)",
			wantErr: NewTypeMismatchError("Integer Vs Bool"),
		},
		{
			name:  "comparison: unequal null vs int",
			input: "(!= null 42)",
			want:  true,
		},
		{
			name:  "comparison: unequal null vs string",
			input: `(!= null "hello")`,
			want:  true,
		},
		{
			name:  "comparison: unequal null vs bool",
			input: "(!= null true)",
			want:  true,
		},
		{
			name:  "comparison: unequal null vs null",
			input: "(!= null null)",
			want:  false,
		},
		{
			name:  "not: negate true",
			input: "(not true)",
			want:  false,
		},
		{
			name:  "not: negate false",
			input: "(not false)",
			want:  true,
		},
		{
			name:  "not: negate true with bang symbol",
			input: "(! true)",
			want:  false,
		},
		{
			name:  "not: negate false with bang symbol",
			input: "(! false)",
			want:  true,
		},
		{
			name:    "not: negate a null",
			input:   "(! null)",
			wantErr: NewTypeMismatchError("expected boolean type, got Null"),
		},
		{
			name:    "not: negate with type mismatch",
			input:   "(not 42)",
			wantErr: NewTypeMismatchError("expected boolean type, got Integer"),
		},
		{
			name:  "less with int operands",
			input: "(< 1 2)",
			want:  true,
		},
		{
			name:  "less with float operands",
			input: "(< 1.5 2.5)",
			want:  true,
		},
		{
			name:  "less with string operands",
			input: `(< "apple" "banana")`,
			want:  true,
		},
		{
			name:    "less with bool operands",
			input:   "(< false true)",
			wantErr: NewUnsupportedOpTypeError("unsupported operation OpLessThan for type Bool"),
		},
		{
			name:  "less with mixed types: int and float",
			input: "(< 1 1.5)",
			want:  true,
		},
		{
			name:  "less or equal with int operands",
			input: "(<= 1 1)",
			want:  true,
		},
		{
			name:  "less or equal with float operands",
			input: "(<= 1.5 1.5)",
			want:  true,
		},
		{
			name:  "less or equal with string operands",
			input: `(<="apple" "banana")`,
			want:  true,
		},
		{
			name:    "less or equal with bool operands",
			input:   "(<= false true)",
			wantErr: NewUnsupportedOpTypeError("unsupported operation OpLessEqual for type Bool"),
		},
		{
			name:  "less or equal with mixed types: int and float",
			input: "(<= 1 1.5)",
			want:  true,
		},
		{
			name:  "greather with int operands",
			input: "(> 2 1)",
			want:  true,
		},
		{
			name:  "greater with float operands",
			input: "(> 2.5 1.5)",
			want:  true,
		},
		{
			name:  "greater with string operands",
			input: `(> "banana" "apple")`,
			want:  true,
		},
		{
			name:    "greater with bool operands",
			input:   "(> true false)",
			wantErr: NewUnsupportedOpTypeError("unsupported operation OpGreaterThan for type Bool"),
		},
		{
			name:  "greater with mixed types: int and float",
			input: "(> 1.5 1)",
			want:  true,
		},
		{
			name:  "greater or equal with int operands",
			input: "(>= 2 2)",
			want:  true,
		},
		{
			name:  "greater or equal with float operands",
			input: "(>= 2.5 2.5)",
			want:  true,
		},
		{
			name:  "greater or equal with string operands",
			input: `(>="banana" "apple")`,
			want:  true,
		},
		{
			name:    "greater or equal with bool operands",
			input:   "(>= true false)",
			wantErr: NewUnsupportedOpTypeError("unsupported operation OpGreaterEqual for type Bool"),
		},
		{
			name:  "greater or equal with mixed types: int and float",
			input: "(>= 1.5 1)",
			want:  true,
		},
		{
			name:  "conditional with matching then branch and undefined else branch",
			input: `(cond true 123)`,
			want:  int64(123),
		},
		{
			name:  "conditional with non-matching then branch",
			input: `(cond false 123)`,
			want:  nil,
		},
		{
			name:  "conditional with matching else branch",
			input: `(cond false 123 456)`,
			want:  int64(456),
		},
		{
			name:  "conditional with no matching then and defined else branch",
			input: `(cond true 123 456)`,
			want:  int64(123),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			assert.NoError(t, err, "Failed to parse input: %s", tt.input)
			comp := compiler.New()
			assert.NoError(t, comp.Compile(prog), "Failed to compile program: %s", tt.input)
			vm := New(comp.Bytecode())

			err = vm.Run()
			if tt.wantErr != nil {
				assert.Error(t, err, "Expected error for input: %s", tt.input)
				assert.EqualError(t, err, tt.wantErr.Error(), "Error message does not match")
				return
			}

			assert.NoError(t, err, "Failed to run program: %s", tt.input)
			st := vm.LastPopped()
			assertObjectEql(t, st, tt.want)
		})
	}
}

func assertObjectEql(t *testing.T, got object.Object, want any) {
	if got == nil {
		assert.Equal(t, want, got, "Expected object to be %v, got %v", want, got)
		return
	}
	switch obj := got.(type) {
	case *object.Integer:
		assert.Equal(t, want, obj.Value, "Expected integer value %v, got %v", want, obj.Value)
	case *object.Float:
		assert.Equal(t, want, obj.Value, "Expected float value %v, got %v", want, obj.Value)
	case *object.String:
		assert.Equal(t, want, obj.Value, "Expected string value %v, got %v", want, obj.Value)
	case *object.Null:
		assert.Equal(t, want, nil, "Expected null object, got %v", obj)
	case *object.Bool:
		assert.Equal(t, want, obj.Value, "Expected boolean value %v, got %v", want, obj.Value)
	default:
		t.Logf("Unimplemented object type: %d", obj.Type())
	}
}

func parse(source string) (*ast.Program, error) {
	lex := lexer.NewLexer(source)
	par := parser.NewParser(lex)
	return par.Parse()
}
