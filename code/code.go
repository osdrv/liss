package code

import (
	"encoding/binary"
	"fmt"
)

type Instructions = []byte

type OpCode = byte

const (
	OpConst OpCode = iota
	OpAdd
)

type Definition struct {
	Name          string
	OperandWidths []int
}

var definitions = map[OpCode]*Definition{
	OpConst: {"OpConst", []int{2}},
	OpAdd:   {"OpAdd", []int{2}},
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
		}
		off += w
	}
	return operands, off
}

func ReadUint16(ins Instructions) uint16 {
	return binary.BigEndian.Uint16(ins)
}
