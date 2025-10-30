package object

import "osdrv/liss/code"

type Module struct {
	defaultObject
	Name   string
	Instrs code.Instructions
}
