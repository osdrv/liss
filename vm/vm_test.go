package vm

import (
	"fmt"
	"osdrv/liss/ast"
	"osdrv/liss/compiler"
	"osdrv/liss/lexer"
	"osdrv/liss/parser"
	"osdrv/liss/test"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
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
			input: `(* "Hello" 3)`,
			want:  "HelloHelloHello",
		},
		{
			name:  "list repetition",
			input: `(* [1 2] 3)`,
			want:  []any{int64(1), int64(2), int64(1), int64(2), int64(1), int64(2)},
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
		{
			name:  "let expression",
			input: `(let x 42)`,
			want:  int64(42),
		},
		{
			name:  "let expression with a resolve",
			input: `(let x 42) x`,
			want:  int64(42),
		},
		{
			name:  "let expression with a nested reference",
			input: `(let y (let x 42))`,
			want:  int64(42),
		},
		{
			name:  "list expression with no elements",
			input: `[]`,
			want:  []any{},
		},
		{
			name:  "list expression with elements",
			input: `[1 "foo" true]`,
			want:  []any{int64(1), "foo", true},
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
			test.AssertObjectEql(t, st, tt.want)
		})
	}
}

func TestFunctionCall(t *testing.T) {
	tests := []struct {
		name    string
		input   string
		want    any
		wantErr error
	}{
		{
			name:  "zero-function",
			input: "((fn []))",
			want:  nil,
		},
		{
			name:  "inline function call with basic arithmetic",
			input: "((fn [] (+ 2 3)))",
			want:  int64(5),
		},
		{
			name:  "inline function call with simple const return",
			input: "((fn [] 42))",
			want:  int64(42),
		},
		{
			name: "named function call",
			input: `
			(fn foo [] (+ 2 3))
			(foo)`,
			want: int64(5),
		},
		{
			name: "named function call chain",
			input: `
			(fn foo [] (+ 2 3))
			(fn bar [] (foo))
			(bar)`,
			want: int64(5),
		},
		{
			name: "arithmetic with results of function calls",
			input: `
			(fn ret2 [] 2)
			(fn ret3 [] 3)
			(fn add [] (+ (ret2) (ret3)))
			(add)`,
			want: int64(5),
		},
		{
			name: "arithmetic with chain function calls",
			input: `
			(fn take_one [] 1)
			(fn mutl_by_2 [] (* (take_one) 2))
			(fn add_3 [] (+ (mutl_by_2) 3))
			(add_3)`,
			want: int64(5),
		},
		{
			name: "function call with resolving a global var",
			input: `
			(let glob 42)
			(fn ret_glob_plus_1 [] (+ glob 1))
			(ret_glob_plus_1)
			`,
			want: int64(43),
		},
		{
			name: "function call with a local binding overriding a global one",
			input: `
			(let foo 42)
			(fn ret_local_foo []
				(let foo 100)
				(+ foo 1))
			(ret_local_foo)
			`,
			want: int64(101),
		},
		{
			name: "function call with a local variable and global bindings",
			input: `
			(let base 10)
			(fn one [] (let x 1) (+ base x))
			(fn two [] (let x 2) (+ base x))
			(+ (one) (two))
			`,
			want: int64(23),
		},
		{
			name: "function call with a local const and global bindings",
			input: `
			(let base 10)
			(fn one [] (- base 1))
			(fn two [] (- base 2))
			(+ (one) (two))
			`,
			want: int64(17),
		},
		{
			name: "function call with one argument",
			input: `
			(fn identity [x] x)
			(identity 42)`,
			want: int64(42),
		},
		{
			name: "function call with multiple arguments",
			input: `
			(fn add [a b c] (+ a b c))
			(add 1 2 3)`,
			want: int64(6),
		},
		{
			name: "nested function call with multiple arguments",
			input: `
			(let a 1)
			(let b 2)
			(let c 3)
			(fn plus1 [x] (+ x 1))
			(fn add_plus_1 [a b c] (+ (plus1 a) (plus1 b) (plus1 c)))
			(add_plus_1 a b c)
			`,
			want: int64(9),
		},
		{
			name: "function call with a wrong number of args",
			input: `
			(fn foo [a] a)
			(foo 1 2)
			`,
			wantErr: fmt.Errorf("Function foo expects 1 arguments, got 2"),
		},
		{
			name:  "concatenate 3 lists",
			input: `(+ [1 2 3] [4 5 6] [7 8 9])`,
			want: []any{int64(1), int64(2), int64(3),
				int64(4), int64(5), int64(6),
				int64(7), int64(8), int64(9)},
		},
		{
			name: "basic dict put",
			input: `
			(let d (dict))
			(put d "key1" 42)
			(get d "key1")
			`,
			want: int64(42),
		},
		{
			name: "dict put, delete and get",
			input: `
			(let d (dict))
			(put d "key1" 42)
			(del d "key1")
			(get d "key1")
			`,
			want: nil,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			require.NoError(t, err, "Failed to parse input: %s", tt.input)
			comp := compiler.New()
			require.NoError(t, comp.Compile(prog), "Failed to compile program: %s", tt.input)
			vm := New(comp.Bytecode())

			err = vm.Run()
			if tt.wantErr != nil {
				assert.Error(t, err, "Expected error for input: %s", tt.input)
				assert.EqualError(t, err, tt.wantErr.Error(), "Error message does not match")
				return
			}

			assert.NoError(t, err, "Failed to run program: %s", tt.input)
			st := vm.LastPopped()
			test.AssertObjectEql(t, st, tt.want)
		})
	}
}

func TestBuiltinCall(t *testing.T) {
	tests := []struct {
		name    string
		input   string
		want    any
		wantErr error
	}{
		{
			name:  "builtin len with a string",
			input: `(len "hello")`,
			want:  int64(5),
		},
		{
			name:  "builtin isNull with null",
			input: `(isNull null)`,
			want:  true,
		},
		{
			name:  "builtin isNull with non-null",
			input: `(isNull 42)`,
			want:  false,
		},
		{
			name:  "builtin variadic list with arguments",
			input: `(list 1 2 3)`,
			want:  []any{int64(1), int64(2), int64(3)},
		},
		{
			name:  "builtin match: simple match",
			input: `(match "h.*o" "hello")`,
			want:  true,
		},
		{
			name:  "builtin match: unicode string",
			input: `(match "пр.*ет" "привет")`,
			want:  true,
		},
		{
			name:  "utf8 strings",
			input: `(+ "Привет, " "мир!")`,
			want:  "Привет, мир!",
		},
		{
			name:  "builtin capture",
			input: `(capture "(a*)b" "aaaaab")`,
			want:  []any{"aaaaab", "aaaaa"},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			prog, err := parse(tt.input)
			require.NoError(t, err, "Failed to parse input: %s", tt.input)
			comp := compiler.New()
			require.NoError(t, comp.Compile(prog), "Failed to compile program: %s", tt.input)
			vm := New(comp.Bytecode())

			err = vm.Run()
			if tt.wantErr != nil {
				assert.Error(t, err, "Expected error for input: %s", tt.input)
				assert.EqualError(t, err, tt.wantErr.Error(), "Error message does not match")
				return
			}

			assert.NoError(t, err, "Failed to run program: %s", tt.input)
			st := vm.LastPopped()
			test.AssertObjectEql(t, st, tt.want)
		})
	}
}

func parse(source string) (ast.Node, error) {
	lex := lexer.NewLexer(source)
	par := parser.NewParser(lex)
	return par.Parse()
}
