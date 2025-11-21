package parser

import (
	"errors"
	"osdrv/liss/ast"
	"osdrv/liss/lexer"
	"osdrv/liss/token"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestParse(t *testing.T) {
	tests := []struct {
		name    string
		input   string
		want    []ast.Node
		wantErr error
	}{
		{
			name:  "Empty input",
			input: "",
			want:  []ast.Node{},
		},
		{
			name:  "Single null literal",
			input: "null",
			want: []ast.Node{
				&ast.NullLiteral{},
			},
		},
		{
			name:  "Single true boolean literal",
			input: "true",
			want: []ast.Node{
				&ast.BooleanLiteral{Value: true},
			},
		},
		{
			name:  "Single false boolean literal",
			input: "false",
			want: []ast.Node{
				&ast.BooleanLiteral{Value: false},
			},
		},
		{
			name:  "Single integer literal",
			input: "42",
			want: []ast.Node{
				&ast.IntegerLiteral{Value: 42},
			},
		},
		{
			name:  "Single signed negative integer literal",
			input: "-42",
			want: []ast.Node{
				&ast.IntegerLiteral{Value: -42},
			},
		},
		{
			name:  "Single signed positive integer literal",
			input: "+42",
			want: []ast.Node{
				&ast.IntegerLiteral{Value: 42},
			},
		},
		{
			name:  "Single float literal",
			input: "3.14",
			want: []ast.Node{
				&ast.FloatLiteral{Value: 3.14},
			},
		},
		{
			name:  "Single signed negative float literal",
			input: "-3.14",
			want: []ast.Node{
				&ast.FloatLiteral{Value: -3.14},
			},
		},
		{
			name:  "Single signed positive float literal",
			input: "+3.14",
			want: []ast.Node{
				&ast.FloatLiteral{Value: 3.14},
			},
		},
		{
			name:  "Single float literal with scientific notation",
			input: "1.23e+04",
			want: []ast.Node{
				&ast.FloatLiteral{Value: 12300.0},
			},
		},
		{
			name:  "Single string literal single quotes",
			input: `'hello world'`,
			want: []ast.Node{
				&ast.StringLiteral{Value: "hello world"},
			},
		},
		{
			name:  "Single string literal double quotes",
			input: `"hello world"`,
			want: []ast.Node{
				&ast.StringLiteral{Value: "hello world"},
			},
		},
		{
			name:  "Single string literal with escaped quotes",
			input: `"hello \"world\""`,
			want: []ast.Node{
				&ast.StringLiteral{Value: `hello "world"`},
			},
		},
		{
			name:  "Single string literal with unicode",
			input: `"おはよう"`,
			want: []ast.Node{
				&ast.StringLiteral{Value: "おはよう"},
			},
		},
		// TODO: Uncomment when unicode escape sequences are supported
		// {
		// 	name:  "Single string literal with escaped unicode",
		// 	input: `"hello \u0041\u0042\u0043"`,
		// 	want: []ast.Node{
		// 		&ast.StringLiteral{Value: "hello ABC"},
		// 	},
		// },
		{
			name:  "Single string literal with newline",
			input: `"hello\nworld"`,
			want: []ast.Node{
				&ast.StringLiteral{Value: "hello\nworld"},
			},
		},
		{
			name:  "Identifier expression",
			input: "myVar",
			want: []ast.Node{
				&ast.IdentifierExpr{Name: "myVar"},
			},
		},
		{
			name:  "Expression with operator",
			input: "(>= 1 2)",
			want: []ast.Node{
				&ast.OperatorExpr{
					Tok: token.Token{
						Type:    token.GreaterThanOrEqual,
						Literal: ">=",
					},
					Operator: ast.OperatorGreaterThanOrEqual,
					Operands: []ast.Node{
						&ast.IntegerLiteral{
							Tok: token.Token{
								Type:    token.Numeric,
								Literal: "1",
							},
							Value: int64(1),
						},
						&ast.IntegerLiteral{
							Tok: token.Token{
								Type:    token.Numeric,
								Literal: "2",
							},
							Value: int64(2),
						},
					},
				},
			},
		},
		{
			name:  "Cond expression with condition and true branch",
			input: "(cond true 123)",
			want: []ast.Node{
				&ast.CondExpression{
					Tok: token.Token{
						Type:    token.Cond,
						Literal: "cond",
					},
					Cond: &ast.BooleanLiteral{
						Tok: token.Token{
							Type:    token.True,
							Literal: "true",
						},
						Value: true,
					},
					Then: &ast.IntegerLiteral{
						Tok: token.Token{
							Type:    token.Numeric,
							Literal: "123",
						},
						Value: int64(123),
					},
				},
			},
		},
		{
			name:  "Cond expression with condition and true/else branches",
			input: "(cond true 123 456)",
			want: []ast.Node{
				&ast.CondExpression{
					Tok: token.Token{
						Type:    token.Cond,
						Literal: "cond",
					},
					Cond: &ast.BooleanLiteral{
						Tok: token.Token{
							Type:    token.True,
							Literal: "true",
						},
						Value: true,
					},
					Then: &ast.IntegerLiteral{
						Tok: token.Token{
							Type:    token.Numeric,
							Literal: "123",
						},
						Value: int64(123),
					},
					Else: &ast.IntegerLiteral{
						Tok: token.Token{
							Type:    token.Numeric,
							Literal: "456",
						},
						Value: int64(456),
					},
				},
			},
		},
		{
			name:    "Cond expression with too many operands",
			input:   "(cond true 123 456 789)",
			wantErr: errors.New("too many operands for cond expression"),
		},
		{
			name:  "Function expression with a name and args",
			input: "(fn add [a b] (+ a b))",
			want: []ast.Node{
				&ast.FunctionExpression{
					Tok: token.Token{
						Type:    token.Fn,
						Literal: "fn",
					},
					Name: &ast.IdentifierExpr{
						Tok: token.Token{
							Type:    token.Identifier,
							Literal: "add",
						},
						Name: "add",
					},
					Args: []*ast.IdentifierExpr{
						&ast.IdentifierExpr{
							Tok: token.Token{
								Type:    token.Identifier,
								Literal: "a",
							},
							Name: "a",
						},
						&ast.IdentifierExpr{
							Tok: token.Token{
								Type:    token.Identifier,
								Literal: "b",
							},
							Name: "b",
						},
					},
					Body: []ast.Node{
						&ast.OperatorExpr{
							Tok: token.Token{
								Type:    token.Plus,
								Literal: "+",
							},
							Operator: ast.OperatorPlus,
							Operands: []ast.Node{
								&ast.IdentifierExpr{
									Tok: token.Token{
										Type:    token.Identifier,
										Literal: "a",
									},
									Name: "a",
								},
								&ast.IdentifierExpr{
									Tok: token.Token{
										Type:    token.Identifier,
										Literal: "b",
									},
									Name: "b",
								},
							},
						},
					},
				},
			},
		},
		{
			name:  "Simple let expression",
			input: "(let x 42)",
			want: []ast.Node{
				&ast.LetExpression{
					Tok: token.Token{
						Type:    token.Let,
						Literal: "let",
					},
					Identifier: &ast.IdentifierExpr{
						Tok: token.Token{
							Type:    token.Identifier,
							Literal: "x",
						},
						Name: "x",
					},
					Value: &ast.IntegerLiteral{
						Tok: token.Token{
							Type:    token.Numeric,
							Literal: "42",
						},
						Value: int64(42),
					},
				},
			},
		},
		{
			name:  "Let expression with sub-expression",
			input: "(let x (+ 1 2))",
			want: []ast.Node{
				&ast.LetExpression{
					Tok: token.Token{
						Type:    token.Let,
						Literal: "let",
					},
					Identifier: &ast.IdentifierExpr{
						Tok: token.Token{
							Type:    token.Identifier,
							Literal: "x",
						},
						Name: "x",
					},
					Value: &ast.OperatorExpr{
						Tok: token.Token{
							Type:    token.Plus,
							Literal: "+",
						},
						Operator: ast.OperatorPlus,
						Operands: []ast.Node{
							&ast.IntegerLiteral{
								Tok: token.Token{
									Type:    token.Numeric,
									Literal: "1",
								},
								Value: int64(1),
							},
							&ast.IntegerLiteral{
								Tok: token.Token{
									Type:    token.Numeric,
									Literal: "2",
								},
								Value: int64(2),
							},
						},
					},
				},
			},
		},
		{
			name:  "Call expression",
			input: "(add 1 2)",
			want: []ast.Node{
				&ast.CallExpression{
					Tok: token.Token{
						Type:    token.Identifier,
						Literal: "add",
					},
					Callee: &ast.IdentifierExpr{
						Tok: token.Token{
							Type:    token.Identifier,
							Literal: "add",
						},
						Name: "add",
					},
					Args: []ast.Node{
						&ast.IntegerLiteral{
							Tok: token.Token{
								Type:    token.Numeric,
								Literal: "1",
							},
							Value: int64(1),
						},
						&ast.IntegerLiteral{
							Tok: token.Token{
								Type:    token.Numeric,
								Literal: "2",
							},
							Value: int64(2),
						},
					},
				},
			},
		},
		{
			name:  "Block expression",
			input: "(1 2 3)",
			want: []ast.Node{
				&ast.BlockExpression{
					Nodes: []ast.Node{
						&ast.IntegerLiteral{
							Tok: token.Token{
								Type:    token.Numeric,
								Literal: "1",
							},
							Value: int64(1),
						},
						&ast.IntegerLiteral{
							Tok: token.Token{
								Type:    token.Numeric,
								Literal: "2",
							},
							Value: int64(2),
						},
						&ast.IntegerLiteral{
							Tok: token.Token{
								Type:    token.Numeric,
								Literal: "3",
							},
							Value: int64(3),
						},
					},
				},
			},
		},
		{
			name:  "List expression",
			input: "[1 2 3]",
			want: []ast.Node{
				&ast.ListExpression{
					Tok: token.Token{
						Type:    token.LBracket,
						Literal: "[",
					},
					Items: []ast.Node{
						&ast.IntegerLiteral{
							Tok: token.Token{
								Type:    token.Numeric,
								Literal: "1",
							},
							Value: int64(1),
						},
						&ast.IntegerLiteral{
							Tok: token.Token{
								Type:    token.Numeric,
								Literal: "2",
							},
							Value: int64(2),
						},
						&ast.IntegerLiteral{
							Tok: token.Token{
								Type:    token.Numeric,
								Literal: "3",
							},
							Value: int64(3),
						},
					},
				},
			},
		},
		{
			name:  "Import expression",
			input: `(import "list")`,
			want: []ast.Node{
				&ast.ImportExpression{
					Tok: token.Token{
						Type:    token.Import,
						Literal: "import",
					},
					Ref: &ast.StringLiteral{
						Tok: token.Token{
							Type:    token.String,
							Literal: `"list"`,
						},
						Value: "list",
					},
				},
			},
		},
		{
			name:  "Import expression with aliasing",
			input: `(import "math" as m)`,
			want: []ast.Node{
				&ast.ImportExpression{
					Tok: token.Token{
						Type:    token.Import,
						Literal: "import",
					},
					Ref: &ast.StringLiteral{
						Tok: token.Token{
							Type:    token.String,
							Literal: `"math"`,
						},
						Value: "math",
					},
					Alias: &ast.IdentifierExpr{
						Tok: token.Token{
							Type:    token.Identifier,
							Literal: "m",
						},
						Name: "m",
					},
				},
			},
		},
		{
			name:  "Import expression with symbols",
			input: `(import "list" ["map" "reduce"])`,
			want: []ast.Node{
				&ast.ImportExpression{
					Tok: token.Token{
						Type:    token.Import,
						Literal: "import",
					},
					Ref: &ast.StringLiteral{
						Tok: token.Token{
							Type:    token.String,
							Literal: `"list"`,
						},
						Value: "list",
					},
					Symbols: &ast.ListExpression{
						Tok: token.Token{
							Type:    token.LBracket,
							Literal: "[",
						},
						Items: []ast.Node{
							&ast.StringLiteral{
								Tok: token.Token{
									Type:    token.String,
									Literal: `"map"`,
								},
								Value: "map",
							},
							&ast.StringLiteral{
								Tok: token.Token{
									Type:    token.String,
									Literal: `"reduce"`,
								},
								Value: "reduce",
							},
						},
					},
				},
			},
		},
		{
			name:  "Valid identifier expression with module",
			input: "math:pi",
			want: []ast.Node{
				&ast.IdentifierExpr{
					Tok: token.Token{
						Type:    token.Identifier,
						Literal: "math:pi",
					},
					Module: "math",
					Name:   "pi",
				},
			},
		},
		{
			name:    "Invalid identifier with multiple colons",
			input:   "math:pi:value",
			wantErr: errors.New("invalid identifier format: \"math:pi:value\""),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			lex := lexer.NewLexer(tt.input)
			par := NewParser(lex)
			prog, err := par.Parse()
			if err != nil || tt.wantErr != nil {
				if tt.wantErr != nil {
					assert.ErrorContains(t, err, tt.wantErr.Error())
				} else {
					assert.NoError(t, err, "Unexpected error: %s", err)
				}
				return
			}
			nodes := prog.(*ast.BlockExpression).Nodes
			assert.Equal(t, tt.wantErr, err, "Expected error %v, got %v", tt.wantErr, err)
			assert.Len(t, nodes, len(tt.want), "Expected %d nodes, got %d", len(tt.want), len(nodes))

			for ix := range tt.want {
				assertNodeEqual(t, tt.want[ix], nodes[ix])
			}
		})
	}
}

func assertNodeEqual(t *testing.T, expected, actual ast.Node) {
	t.Helper()
	if expected == nil && actual == nil {
		return
	}
	if expected == nil || actual == nil {
		t.Fatalf("Expected node to be %T, got %T", expected, actual)
	}
	t.Logf("Expected: %s, Actual: %s", expected.String(), actual.String())
	assert.Equal(t, expected.String(), actual.String(), "Node strings do not match")
}
