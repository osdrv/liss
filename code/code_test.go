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
		{
			name:     "OpPop with no operands",
			op:       OpPop,
			operands: []int{},
			expected: Instructions{OpPop},
		},
		{
			name:     "OpGetLocal with one operand",
			op:       OpGetLocal,
			operands: []int{255},
			expected: Instructions{OpGetLocal, 255},
		},
		{
			name:     "OpClosure with two operands",
			op:       OpClosure,
			operands: []int{65535, 255},
			expected: Instructions{OpClosure, 255, 255, 255},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			instr := Make(tt.op, tt.operands...)
			assert.Equal(t, tt.expected, instr)
		})
	}
}

func TestReadOperands(t *testing.T) {
	tests := []struct {
		name      string
		op        OpCode
		operands  []int
		wantBytes int
	}{
		{
			name:      "OpConst with one operand",
			op:        OpConst,
			operands:  []int{65535},
			wantBytes: 2,
		},
		{
			name:      "OpGetLocal with one operand",
			op:        OpGetLocal,
			operands:  []int{255},
			wantBytes: 1,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			instr := Make(tt.op, tt.operands...)
			def, err := Lookup(tt.op)
			assert.NoError(t, err, "Lookup should not return an error for valid opcode")
			opsRead, bytesRead := ReadOperands(def, instr[1:])
			assert.Equal(t, tt.wantBytes, bytesRead, "Bytes read should match expected")
			assert.Equal(t, tt.operands, opsRead, "Operands read should match expected")
		})
	}
}

func TestPrintInstr(t *testing.T) {
	instrs := []Instructions{
		Make(OpConst, 1),
		Make(OpConst, 2),
		Make(OpConst, 65535),
		Make(OpClosure, 65535, 255),
	}

	expected := `0000 OpConst 1
0003 OpConst 2
0006 OpConst 65535
0009 OpClosure 65535 255
`
	conc := Instructions{}
	for _, instr := range instrs {
		conc = append(conc, instr...)
	}

	assert.Equal(t, expected, PrintInstr(conc))
}
