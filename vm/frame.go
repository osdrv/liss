package vm

import (
	"osdrv/liss/code"
	"osdrv/liss/object"
)

type Frame struct {
	fn   *object.Function
	ip   int
	bptr int
}

func NewFrame(fn *object.Function, bptr int) *Frame {
	return &Frame{
		fn:   fn,
		ip:   -1,
		bptr: bptr,
	}
}

func (f *Frame) Instructions() code.Instructions {
	return f.fn.Instrs
}
