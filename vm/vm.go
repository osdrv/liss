package vm

import (
	"errors"
	"osdrv/liss/code"
	"osdrv/liss/compiler"
	"osdrv/liss/object"
)

const StackSize = 2048

var ErrStackOverflow = errors.New("stack overflow")

type VM struct {
	consts []object.Object
	instrs code.Instructions

	stack []object.Object
	pc    int // program counter
}

func New(bc *compiler.Bytecode) *VM {
	return &VM{
		consts: bc.Consts,
		instrs: bc.Instrs,
		stack:  make([]object.Object, StackSize),
		pc:     0,
	}
}

func (vm *VM) StackTop() object.Object {
	if vm.pc == 0 {
		return nil
	}
	return vm.stack[vm.pc-1]
}

func (vm *VM) Run() error {
	for ip := 0; ip < len(vm.instrs); ip++ {
		op := vm.instrs[ip]
		switch op {
		case code.OpConst:
			ix := code.ReadUint16(vm.instrs[ip+1:])
			ip += 2
			err := vm.push(vm.consts[ix])
			if err != nil {
				return err
			}
		case code.OpAdd:
			argc := code.ReadUint16(vm.instrs[ip+1:])
			ip += 2
			// TODO: handle different types
			sum := int64(0)
			for range argc {
				sum += int64(vm.pop().(*object.Integer).Value)
			}
			vm.push(object.NewInteger(sum))
		}
	}

	return nil
}

func (vm *VM) push(obj object.Object) error {
	if vm.pc >= StackSize {
		return ErrStackOverflow
	}
	vm.stack[vm.pc] = obj
	vm.pc++

	return nil
}

func (vm *VM) pop() object.Object {
	obj := vm.stack[vm.pc-1]
	vm.pc--
	return obj
}
