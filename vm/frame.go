package vm

import (
	"osdrv/liss/code"
	"osdrv/liss/object"
)

type Frame struct {
	fn *object.Function
	ip int
}

func NewFrame(fn *object.Function) *Frame {
	return &Frame{
		fn: fn,
		ip: -1,
	}
}

func (f *Frame) Instructions() code.Instructions {
	return f.fn.Instrs
}
