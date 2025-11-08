package regexp

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestCompile(t *testing.T) {
	tests := []struct {
		name      string
		inputNode *ASTNode
		wantNext  int
		wantInsts []Inst
	}{
		{
			name: "Literal 'a'",
			inputNode: &ASTNode{
				Type:  NodeTypeLiteral,
				Value: "a",
			},
			wantInsts: []Inst{
				{Op: ReOpRune, Rune: 'a'},
			},
		},
		{
			name: "Concatenation 'ab'",
			inputNode: &ASTNode{
				Type: NodeTypeConcat,
				Children: []*ASTNode{
					{Type: NodeTypeLiteral, Value: "a"},
					{Type: NodeTypeLiteral, Value: "b"},
				},
			},
			wantInsts: []Inst{
				{Op: ReOpRune, Rune: 'b'},
				{Op: ReOpRune, Rune: 'a'},
			},
		},
		{
			name: "rune class digit",
			inputNode: &ASTNode{
				Type:  NodeTypeClass,
				Value: "d",
			},
			wantInsts: []Inst{
				{Op: ReOpRuneClass, Arg: RuneClassDigit},
			},
		},
		{
			name: "rune class digit with * repetitor",
			inputNode: &ASTNode{
				Type: NodeTypeStar,
				Children: []*ASTNode{
					{Type: NodeTypeClass, Value: "d"},
				},
			},
			wantInsts: []Inst{
				{Op: ReOpSplit, Out: 0, Out1: 1},
				{Op: ReOpRuneClass, Arg: RuneClassDigit},
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			c := &compiler{prog: &Prog{}}
			c.compile(tt.inputNode, 0)
			assert.Equal(t, tt.wantInsts, c.prog.Insts)
		})
	}
}
