package main

import (
	"osdrv/liss/object"
	"osdrv/liss/repl"
	"osdrv/liss/test"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// High-level Liss tests
func TestExecute(t *testing.T) {
	tests := []struct {
		name    string
		source  string
		want    any
		wantErr error
	}{
		{
			name:   "Integer literal",
			source: "42",
			want:   int64(42),
		},
		{
			name:   "Negative integer literal",
			source: "-42",
			want:   int64(-42),
		},
		{
			name:   "Floating point literal",
			source: "3.14e-2",
			want:   float64(3.14e-2),
		},
		{
			name:   "Negative floating point literal",
			source: "-3.14e+2",
			want:   float64(-3.14e+2),
		},
		{
			name:   "String literal single quotes",
			source: "'hello'",
			want:   "hello",
		},
		{
			name:   "String literal double quotes",
			source: `"hello"`,
			want:   "hello",
		},
		{
			name:   "Boolean true literal",
			source: "true",
			want:   true,
		},
		{
			name:   "Boolean false literal",
			source: "false",
			want:   false,
		},
		{
			name:   "Null literal",
			source: "null",
			want:   nil,
		},
		{
			name:   "Addition with integer operands",
			source: "(+ 1 2)",
			want:   int64(3),
		},
		{
			name:   "Addition with floating point operands",
			source: "(+ 1.5 2.5)",
			want:   float64(4.0),
		},
		{
			name:   "Addition with string operands (concatenation)",
			source: "(+ 'hello' ' world')",
			want:   "hello world",
		},
		{
			name:   "Subtraction with integer operands",
			source: "(- 5 3)",
			want:   int64(2),
		},
		{
			name:   "Subtraction with floating point operands",
			source: "(- 5.5 2.5)",
			want:   float64(3.0),
		},
		{
			name:   "Multiplication with integer operands",
			source: "(* 4 2)",
			want:   int64(8),
		},
		{
			name:   "Multiplication with floating point operands",
			source: "(* 3.0 2.0)",
			want:   float64(6.0),
		},
		{
			name:   "Multiplication with string operands (repetition)",
			source: "(* 'abc' 3)",
			want:   "abcabcabc",
		},
		{
			name:   "Multiplication with list (repetition)",
			source: "(* (list 1 2) 3)",
			want:   []any{int64(1), int64(2), int64(1), int64(2), int64(1), int64(2)},
		},
		{
			name:   "Division with integer operands",
			source: "(/ 8 2)",
			want:   int64(4),
		},
		{
			name:   "Division with floating point operands",
			source: "(/ 7.0 2.0)",
			want:   float64(3.5),
		},
		{
			name:   "Modulus with integer operands",
			source: "(% 10 3)",
			want:   int64(1),
		},
		{
			name:   "Logical AND with boolean operands",
			source: "(& true false)",
			want:   false,
		},
		{
			name:   "Logical OR with boolean operands",
			source: "(| true false)",
			want:   true,
		},
		{
			name:   "Logical NOT with boolean operand",
			source: "(! true)",
			want:   false,
		},
		{
			name:   "Equality with integer operands",
			source: "(= 42 42)",
			want:   true,
		},
		{
			// This might return false if the operands are chosen such that
			// they are not exactly equal due to floating point precision issues.
			name:   "Equality with floating point operands",
			source: "(= 3.14 3.14)",
			want:   true,
		},
		{
			name:   "Equality with string operands",
			source: "(= 'hello' 'hello')",
			want:   true,
		},
		{
			name:   "Equality with boolean operands",
			source: "(= true true)",
			want:   true,
		},
		{
			name:   "Inequality with integer operands",
			source: "(!= 42 43)",
			want:   true,
		},
		{
			name:   "Inequality with floating point operands",
			source: "(!= 3.14 2.71)",
			want:   true,
		},
		{
			name:   "Inequality with string operands",
			source: "(!= 'hello' 'world')",
			want:   true,
		},
		{
			name:   "Inequality with boolean operands",
			source: "(!= true false)",
			want:   true,
		},
		{
			name:   "Less than with integer operands",
			source: "(< 5 10)",
			want:   true,
		},
		{
			name:   "Less than with floating point operands",
			source: "(< 3.14 4.0)",
			want:   true,
		},
		{
			name:   "Less than with string operands",
			source: "(< 'abc' 'bcd')",
			want:   true,
		},
		{
			name:   "Less than or equal with integer operands",
			source: "(<= 5 5)",
			want:   true,
		},
		{
			name:   "Less than or equal with floating point operands",
			source: "(<= 3.14 3.14)",
			want:   true,
		},
		{
			name:   "Less than or equal with string operands",
			source: "(<= 'abc' 'bcd')",
			want:   true,
		},
		{
			name:   "Greater than with integer operands",
			source: "(> 10 5)",
			want:   true,
		},
		{
			name:   "Greater than with floating point operands",
			source: "(> 4.0 3.14)",
			want:   true,
		},
		{
			name:   "Greater than with string operands",
			source: "(> 'bcd' 'abc')",
			want:   true,
		},
		{
			name:   "Greater than or equal with integer operands",
			source: "(>= 5 5)",
			want:   true,
		},
		{
			name:   "Greater than or equal with floating point operands",
			source: "(>= 3.14 3.14)",
			want:   true,
		},
		{
			name:   "Greater than or equal with string operands",
			source: "(>= 'bcd' 'abc')",
			want:   true,
		},
		{
			name: "Extract capture group from regex match",
			source: `
				(let capts (re:capture "10 (.*) lines" "10 blue lines"))
				(get capts 1)
				`,
			want: "blue",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			res, err := Execute(tt.source, repl.Options{})
			if err != nil || tt.wantErr != nil {
				if tt.wantErr != nil {
					assert.ErrorContains(t, err, tt.wantErr.Error())
				} else {
					require.NoError(t, err, "Unexpected error for source: %s", tt.source)
				}
				return
			}
			assert.Equal(t, tt.wantErr, err, "Expected error %v, got %v", tt.wantErr, err)
			test.AssertObjectEql(t, res.(object.Object), tt.want)
			//assert.Equal(t, tt.want, res, "Expected result %v, got %v", tt.want, res)
		})
	}
}
