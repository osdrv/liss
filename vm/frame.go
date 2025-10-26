package vm

import (
	"osdrv/liss/code"
	"osdrv/liss/object"
)

type Frame struct {
	cl   *object.Closure
	ip   int
	bptr int
}

func NewFrame(cl *object.Closure, bptr int) *Frame {
	return &Frame{
		cl:   cl,
		ip:   -1,
		bptr: bptr,
	}
}

func (f *Frame) Instructions() code.Instructions {
	return f.cl.Fn.Instrs
}
