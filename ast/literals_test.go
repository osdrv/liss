package ast

import (
	"errors"
	"osdrv/liss/token"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestIntegerLiteral(t *testing.T) {
	tests := []struct {
		name    string
		input   token.Token
		want    Node
		wantErr error
	}{
		{
			name: "Valid integer literal",
			input: token.Token{
				Type:    token.Numeric,
				Literal: "42",
			},
			want: &IntegerLiteral{
				Tok: token.Token{
					Type:    token.Numeric,
					Literal: "42",
				},
				Value: 42,
			},
		},
		{
			name: "Invalid token type",
			input: token.Token{
				Type:    token.String,
				Literal: "not a number",
			},
			wantErr: errors.New("expected token type Numeric, got String"),
		},
		{
			name: "Hexadecimal integer literal",
			input: token.Token{
				Type:    token.Numeric,
				Literal: "0x2A",
			},
			want: &IntegerLiteral{
				Tok: token.Token{
					Type:    token.Numeric,
					Literal: "0x2A",
				},
				Value: 42,
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := NewIntegerLiteral(tt.input)
			if tt.wantErr != nil {
				assert.ErrorContains(t, err, tt.wantErr.Error())
				return
			}
			if err != nil {
				t.Fatalf("NewIntegerLiteral() error = %v, wantErr %v", err, tt.wantErr)
			}
			assert.Equal(t, tt.want, got)
		})
	}
}

func TestFloatLiteral(t *testing.T) {
	tests := []struct {
		name    string
		input   token.Token
		want    Node
		wantErr error
	}{
		{
			name: "Valid float literal",
			input: token.Token{
				Type:    token.Numeric,
				Literal: "3.14",
			},
			want: &FloatLiteral{
				Tok: token.Token{
					Type:    token.Numeric,
					Literal: "3.14",
				},
				Value: 3.14,
			},
		},
		{
			name: "Invalid token type",
			input: token.Token{
				Type:    token.String,
				Literal: "not a float",
			},
			wantErr: errors.New("expected token type Numeric, got String"),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := NewFloatLiteral(tt.input)
			if tt.wantErr != nil {
				assert.ErrorContains(t, err, tt.wantErr.Error())
				return
			}
			if err != nil {
				t.Fatalf("NewFloatLiteral() error = %v, wantErr %v", err, tt.wantErr)
			}
			assert.Equal(t, tt.want, got)
		})
	}
}

func TestStringLiteral(t *testing.T) {
	tests := []struct {
		name    string
		input   token.Token
		want    Node
		wantErr error
	}{
		{
			name: "Valid string literal",
			input: token.Token{
				Type:    token.String,
				Literal: `Hello, World!`,
			},
			want: &StringLiteral{
				Tok: token.Token{
					Type:    token.String,
					Literal: `Hello, World!`,
				},
				Value: `Hello, World!`,
			},
		},
		{
			name: "Invalid token type",
			input: token.Token{
				Type:    token.Numeric,
				Literal: "not a string",
			},
			wantErr: errors.New("expected token type String, got Numeric"),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := NewStringLiteral(tt.input)
			if tt.wantErr != nil {
				assert.ErrorContains(t, err, tt.wantErr.Error())
				return
			}
			if err != nil {
				t.Fatalf("NewStringLiteral() error = %v, wantErr %v", err, tt.wantErr)
			}
			assert.Equal(t, tt.want, got)
		})
	}
}

func TestBooleanLiteral(t *testing.T) {
	tests := []struct {
		name    string
		input   token.Token
		want    Node
		wantErr error
	}{
		{
			name: "Valid boolean literal true",
			input: token.Token{
				Type:    token.True,
				Literal: "true",
			},
			want: &BooleanLiteral{
				Tok: token.Token{
					Type:    token.True,
					Literal: "true",
				},
				Value: true,
			},
		},
		{
			name: "Valid boolean literal false",
			input: token.Token{
				Type:    token.False,
				Literal: "false",
			},
			want: &BooleanLiteral{
				Tok: token.Token{
					Type:    token.False,
					Literal: "false",
				},
				Value: false,
			},
		},
		{
			name: "Invalid token type",
			input: token.Token{
				Type:    token.Numeric,
				Literal: "not a boolean",
			},
			wantErr: errors.New("expected token type Boolean, got Numeric"),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := NewBooleanLiteral(tt.input)
			if tt.wantErr != nil {
				assert.ErrorContains(t, err, tt.wantErr.Error())
				return
			}
			if err != nil {
				t.Fatalf("NewBooleanLiteral() error = %v, wantErr %v", err, tt.wantErr)
			}
			assert.Equal(t, tt.want, got)
		})
	}
}
