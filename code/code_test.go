package code

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestMake(t *testing.T) {
	tests := []struct {
		name     string
		op       OpCode
		operands []int
		expected Instructions
	}{
		{
			name:     "OpConst with one operand",
			op:       OpConst,
			operands: []int{65534},
			expected: Instructions{OpConst, 255, 254},
		},
		{
			name:     "OpAdd",
			op:       OpAdd,
			operands: []int{2},
			expected: Instructions{OpAdd, 0, 2},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			instr := Make(tt.op, tt.operands...)
			assert.Equal(t, tt.expected, instr)

		})
	}
}
