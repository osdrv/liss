package ast

import (
	"errors"
	"osdrv/liss/token"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestNewIdentifierExpr(t *testing.T) {
	tests := []struct {
		name    string
		input   token.Token
		want    *IdentifierExpr
		wantErr error
	}{
		{
			name:  "Valid identifier",
			input: token.Token{Type: token.Identifier, Literal: "foo"},
			want:  &IdentifierExpr{Tok: token.Token{Type: token.Identifier, Literal: "foo"}, Name: "foo"},
		},
		{
			name:    "Invalid token type",
			input:   token.Token{Type: token.Numeric, Literal: "42"},
			wantErr: errors.New("expected token type Identifier, got Numeric"),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := NewIdentifierExpr(tt.input)
			if err != nil || tt.wantErr != nil {
				if tt.wantErr != nil {
					assert.ErrorContains(t, err, tt.wantErr.Error())
				} else {
					assert.NoError(t, err, "Unexpected error: %s", err)
				}
				return
			}
			assert.Equal(t, tt.wantErr, err, "Expected error %v, got %v", tt.wantErr, err)
			assert.Equal(t, tt.want, got, "Expected result %v, got %v", tt.want, got)
		})
	}
}
