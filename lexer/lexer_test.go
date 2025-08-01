package lexer

import (
	"osdrv/liss/token"
	"testing"

	"github.com/stretchr/testify/assert"
)

type TokenMatch uint8

const (
	TokenTypeMatch TokenMatch = 1 << iota
	TokenLocationMatch
	TokenLiteralMatch
)

func TestNextToken(t *testing.T) {
	tests := []struct {
		name    string
		input   string
		match   TokenMatch
		want    []token.Token
		wantErr error
	}{
		{
			name:  "Empty input",
			input: "",
			match: TokenTypeMatch | TokenLocationMatch,
			want: []token.Token{token.Token{
				Type: token.EOF,
				Location: token.Location{
					Line:   1,
					Column: 1,
				},
			}},
		},
		{
			name:  "Whitespace no newline",
			input: "   ",
			match: TokenTypeMatch | TokenLocationMatch,
			want: []token.Token{token.Token{
				Type: token.EOF,
				Location: token.Location{
					Line:   1,
					Column: 4,
				},
			}},
		},
		{
			name:  "Whitespace with newline",
			input: "   \n",
			match: TokenTypeMatch | TokenLocationMatch,
			want: []token.Token{token.Token{
				Type: token.EOF,
				Location: token.Location{
					Line:   2,
					Column: 1,
				},
			}},
		},
		{
			name:  "Numeric int literal",
			input: "42",
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.Numeric,
					Literal: "42",
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 3,
					},
				},
			},
		},
		{
			name:  "Numeric float literal simple decimal notation",
			input: "42.0123",
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.Numeric,
					Literal: "42.0123",
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 8,
					},
				},
			},
		},
		{
			name:  "Numeric float literal scientific notation",
			input: "42.0123e10",
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.Numeric,
					Literal: "42.0123e10",
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 11,
					},
				},
			},
		},
		{
			name:  "Numeric negative int literal",
			input: "-42",
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.Numeric,
					Literal: "-42",
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 4,
					},
				},
			},
		},
		{
			name:  "Numeric positive int literal",
			input: "+42",
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.Numeric,
					Literal: "+42",
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 4,
					},
				},
			},
		},
		{
			name:  "Numeric float literal with exponent with sign",
			input: "+42.0123e+10",
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.Numeric,
					Literal: "+42.0123e+10",
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 13,
					},
				},
			},
		},
		{
			name:  "Multiple numeric literals",
			input: "42 +3.14 -2e-5",
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.Numeric,
					Literal: "42",
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type:    token.Numeric,
					Literal: "+3.14",
					Location: token.Location{
						Line:   1,
						Column: 4,
					},
				},
				{
					Type:    token.Numeric,
					Literal: "-2e-5",
					Location: token.Location{
						Line:   1,
						Column: 10,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 15,
					},
				},
			},
		},
		{
			name:  "String literal single quotes",
			input: "'hello'",
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.String,
					Literal: "hello",
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 8,
					},
				},
			},
		},
		{
			name:  "String literal single quotes",
			input: "'hello'",
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.String,
					Literal: "hello",
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 8,
					},
				},
			},
		},
		{
			name:  "String literal double quotes",
			input: `"hello"`,
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.String,
					Literal: "hello",
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 8,
					},
				},
			},
		},
		{
			name:  "String literal double quotes with escape character",
			input: `"hello \"world\""`,
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.String,
					Literal: `hello "world"`,
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 18,
					},
				},
			},
		},
		{
			name:  "String literal with escaped newline",
			input: `"hello\nworld"`,
			match: TokenTypeMatch | TokenLocationMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.String,
					Literal: "hello\nworld",
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   1,
						Column: 15,
					},
				},
			},
		},
		{
			name:  "Single character operators",
			input: "+ - * / % & | =",
			match: TokenTypeMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.Plus,
					Literal: "+",
				},
				{
					Type:    token.Minus,
					Literal: "-",
				},
				{
					Type:    token.Multiply,
					Literal: "*",
				},
				{
					Type:    token.Divide,
					Literal: "/",
				},
				{
					Type:    token.Modulus,
					Literal: "%",
				},
				{
					Type:    token.And,
					Literal: "&",
				},
				{
					Type:    token.Or,
					Literal: "|",
				},
				{
					Type:    token.Equal,
					Literal: "=",
				},
				{
					Type: token.EOF,
				},
			},
		},
		{
			name:  "Multi-character operators",
			input: "!= <= >=",
			match: TokenTypeMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.NotEqual,
					Literal: "!=",
				},
				{
					Type:    token.LessThanOrEqual,
					Literal: "<=",
				},
				{
					Type:    token.GreaterThanOrEqual,
					Literal: ">=",
				},
				{
					Type: token.EOF,
				},
			},
		},
		{
			name:  "Parenthesis",
			input: "(\n)",
			match: TokenTypeMatch | TokenLocationMatch,
			want: []token.Token{
				{
					Type: token.LParen,
					Location: token.Location{
						Line:   1,
						Column: 1,
					},
				},
				{
					Type: token.RParen,
					Location: token.Location{
						Line:   2,
						Column: 1,
					},
				},
				{
					Type: token.EOF,
					Location: token.Location{
						Line:   2,
						Column: 2,
					},
				},
			},
		},
		{
			name:  "Keywords",
			input: "fn true false null cond let and or not",
			match: TokenTypeMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.Fn,
					Literal: "fn",
				},
				{
					Type:    token.True,
					Literal: "true",
				},
				{
					Type:    token.False,
					Literal: "false",
				},
				{
					Type:    token.Null,
					Literal: "null",
				},
				{
					Type:    token.Cond,
					Literal: "cond",
				},
				{
					Type:    token.Let,
					Literal: "let",
				},
				{
					Type:    token.And,
					Literal: "and",
				},
				{
					Type:    token.Or,
					Literal: "or",
				},
				{
					Type:    token.Not,
					Literal: "not",
				},
				{
					Type: token.EOF,
				},
			},
		},
		{
			name:  "Identifiers",
			input: "foo bar baz _underscore123 a b_c d3f trueee",
			match: TokenTypeMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.Identifier,
					Literal: "foo",
				},
				{
					Type:    token.Identifier,
					Literal: "bar",
				},
				{
					Type:    token.Identifier,
					Literal: "baz",
				},
				{
					Type:    token.Identifier,
					Literal: "_underscore123",
				},
				{
					Type:    token.Identifier,
					Literal: "a",
				},
				{
					Type:    token.Identifier,
					Literal: "b_c",
				},
				{
					Type:    token.Identifier,
					Literal: "d3f",
				},
				{
					Type:    token.Identifier,
					Literal: "trueee",
				},
				{
					Type: token.EOF,
				},
			},
		},
		{
			name:  "Accessors",
			input: ".foo.bar[0].baz .baz2 .[1].qux .[2][3] .",
			match: TokenTypeMatch | TokenLiteralMatch,
			want: []token.Token{
				{
					Type:    token.Accessor,
					Literal: ".foo.bar[0].baz",
				},
				{
					Type:    token.Accessor,
					Literal: ".baz2",
				},
				{
					Type:    token.Accessor,
					Literal: ".[1].qux",
				},
				{
					Type:    token.Accessor,
					Literal: ".[2][3]",
				},
				{
					Type:    token.Accessor,
					Literal: ".",
				},
				{
					Type: token.EOF,
				},
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			lex := NewLexer(tt.input)
			got := make([]token.Token, 0)
			for {
				tok := lex.NextToken()
				got = append(got, tok)
				if tok.Type == token.EOF || tok.Type == token.Error {
					break
				}
			}
			assert.ErrorIs(t, lex.Err(), tt.wantErr)
			assert.Len(t, got, len(tt.want))

			for ix := range got {
				gott := got[ix]
				wantt := tt.want[ix]

				if tt.match&TokenTypeMatch != 0 {
					assert.Equal(t, wantt.Type, gott.Type, "Token type mismatch at index %d", ix)
				}
				if tt.match&TokenLocationMatch != 0 {
					assert.Equal(t, wantt.Location, gott.Location, "Token location mismatch at index %d", ix)
				}
				if tt.match&TokenLiteralMatch != 0 {
					assert.Equal(t, wantt.Literal, gott.Literal, "Token literal mismatch at index %d", ix)
				}
			}
		})
	}
}
