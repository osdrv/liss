package code

import (
	"bytes"
	"encoding/binary"
	"fmt"
)

type Instructions = []byte

type OpCode = byte

const (
	_ OpCode = iota
	OpConst
	OpTrue
	OpFalse
	OpNull

	OpAdd
	OpSub
	OpMul
	OpDiv

	OpEql
	OpNotEql
	OpGreaterThan
	OpGreaterEqual
	OpLessThan
	OpLessEqual
	OpNot
	OpAnd
	OpOr

	OpPop
	OpJump
	OpJumpIfFalse
	OpGetGlobal
	OpSetGlobal
	OpGetLocal
	OpSetLocal
	OpGetBuiltin
	OpCall
	OpReturn
)

type Definition struct {
	Name          string
	OperandWidths []int
}

var definitions = map[OpCode]*Definition{
	OpConst: {"OpConst", []int{2}},
	OpTrue:  {"OpTrue", []int{}},
	OpFalse: {"OpFalse", []int{}},
	OpNull:  {"OpNull", []int{}},

	OpAdd: {"OpAdd", []int{2}},
	OpSub: {"OpSub", []int{}},
	OpMul: {"OpMul", []int{2}},
	OpDiv: {"OpDiv", []int{}},

	OpEql:          {"OpEql", []int{}},
	OpNotEql:       {"OpNotEql", []int{}},
	OpGreaterThan:  {"OpGreaterThan", []int{}},
	OpGreaterEqual: {"OpGreaterEqual", []int{}},
	OpLessThan:     {"OpLessThan", []int{}},
	OpLessEqual:    {"OpLessEqual", []int{}},
	OpNot:          {"OpNot", []int{}},
	OpAnd:          {"OpAnd", []int{}},
	OpOr:           {"OpOr", []int{}},

	OpPop:         {"OpPop", []int{}},
	OpJump:        {"OpJump", []int{2}},
	OpJumpIfFalse: {"OpJumpIfFalse", []int{2}},
	OpGetGlobal:   {"OpGetGlobal", []int{2}},
	OpSetGlobal:   {"OpSetGlobal", []int{2}},
	OpGetLocal:    {"OpGetLocal", []int{1}},
	OpSetLocal:    {"OpSetLocal", []int{1}},
	OpGetBuiltin:  {"OpGetBuiltin", []int{1}},
	OpCall:        {"OpCall", []int{1}},
	OpReturn:      {"OpReturn", []int{}},
}

func Lookup(op OpCode) (*Definition, error) {
	def, ok := definitions[OpCode(op)]
	if !ok {
		return nil, fmt.Errorf("opcode %d is undefined", op)
	}
	return def, nil
}

func Make(op OpCode, operands ...int) []byte {
	def, ok := definitions[op]
	if !ok {
		return []byte{}
	}
	if len(operands) != len(def.OperandWidths) {
		panic(
			fmt.Sprintf(
				"Wrong number of arguments: opCode: %s, want: %d, got: %d",
				def.Name,
				len(def.OperandWidths),
				len(operands),
			),
		)
	}
	instrlen := 1
	for _, w := range def.OperandWidths {
		instrlen += w
	}

	instr := make([]byte, instrlen)
	instr[0] = byte(op)

	off := 1
	for i, o := range operands {
		w := def.OperandWidths[i]
		switch w {
		case 2:
			binary.BigEndian.PutUint16(instr[off:], uint16(o))
		case 1:
			instr[off] = byte(o)
		}
		off += w
	}
	return instr
}

func ReadOperands(def *Definition, ins Instructions) ([]int, int) {
	operands := make([]int, len(def.OperandWidths))
	off := 0
	for i, w := range def.OperandWidths {
		switch w {
		case 2:
			operands[i] = int(ReadUint16(ins[off:]))
		case 1:
			operands[i] = int(ReadUint8(ins[off:]))
		}
		off += w
	}
	return operands, off
}

func ReadUint16(ins Instructions) uint16 {
	return binary.BigEndian.Uint16(ins)
}

func ReadUint8(ins Instructions) uint8 {
	return uint8(ins[0])
}

func PrintInstr(instr Instructions) string {
	var b bytes.Buffer

	off := 0
	for off < len(instr) {
		def, err := Lookup(OpCode(instr[off]))
		if err != nil {
			b.WriteString(fmt.Sprintf("ERROR: %s\n", err))
			continue
		}
		ops, size := ReadOperands(def, instr[off+1:])
		b.WriteString(fmt.Sprintf("%04d %s\n", off, fmtInstr(def, ops)))
		off += 1 + size
	}

	return b.String()
}

func PrintOpCode(op OpCode) string {
	if def, ok := definitions[op]; ok {
		return def.Name
	}
	return fmt.Sprintf("UNKNOWN(%d)", op)
}

func fmtInstr(def *Definition, ops []int) string {
	opscnt := len(def.OperandWidths)
	if len(ops) != opscnt {
		return fmt.Sprintf("ERROR: expected %d operands, got %d", opscnt, len(ops))
	}
	switch opscnt {
	case 0:
		return def.Name
	case 1:
		return fmt.Sprintf("%s %d", def.Name, ops[0])
	}

	return fmt.Sprintf("ERROR: unhandled operand count for %s", def.Name)
}
