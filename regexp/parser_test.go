package regexp

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestParseAlternate(t *testing.T) {
	tests := []struct {
		name    string
		input   string
		want    *ASTNode
		wantErr error
	}{
		{
			name:  "single rune",
			input: "a",
			want: &ASTNode{
				Type:  NodeTypeLiteral,
				Value: "a",
			},
		},
		{
			name:  "simple alternation",
			input: "a|b",
			want: &ASTNode{
				Type: NodeTypeAlternation,
				Children: []*ASTNode{
					&ASTNode{
						Type:  NodeTypeLiteral,
						Value: "a",
					},
					&ASTNode{
						Type:  NodeTypeLiteral,
						Value: "b",
					},
				},
			},
		},
		{
			name:  "rune class digit",
			input: "\\d",
			want: &ASTNode{
				Type:  NodeTypeClass,
				Value: "d",
			},
		},
		{
			name:  "rune class digit with + repetitor",
			input: "\\d+",
			want: &ASTNode{
				Type: NodeTypePlus,
				Children: []*ASTNode{
					&ASTNode{
						Type:  NodeTypeClass,
						Value: "d",
					},
				},
			},
		},
		{
			name:  "rune class digit with * repetitor",
			input: "\\d*",
			want: &ASTNode{
				Type: NodeTypeStar,
				Children: []*ASTNode{
					&ASTNode{
						Type:  NodeTypeClass,
						Value: "d",
					},
				},
			},
		},
		{
			name:  "escaped parenthesis",
			input: "\\(\\d+\\)",
			want: &ASTNode{
				Type: NodeTypeConcat,
				Children: []*ASTNode{
					&ASTNode{
						Type:  NodeTypeLiteral,
						Value: "(",
					},
					&ASTNode{
						Type: NodeTypePlus,
						Children: []*ASTNode{
							&ASTNode{
								Type:  NodeTypeClass,
								Value: "d",
							},
						},
					},
					&ASTNode{
						Type:  NodeTypeLiteral,
						Value: ")",
					},
				},
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			p := &parser{
				input: []rune(tt.input),
			}
			got, err := p.parseAlternate()
			if tt.wantErr != nil {
				require.ErrorContains(t, err, tt.wantErr.Error())
				return
			} else {
				require.NoError(t, err)
			}
			assert.Equal(t, tt.want, got)
		})
	}
}
