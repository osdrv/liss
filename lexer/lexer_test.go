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
