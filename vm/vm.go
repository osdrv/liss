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
		case code.OpSub:
			b := vm.pop()
			a := vm.pop()
			res := int64(a.(*object.Integer).Value) - int64(b.(*object.Integer).Value)
			vm.push(object.NewInteger(res))
		case code.OpMul:
			argc := code.ReadUint16(vm.instrs[ip+1:])
			ip += 2
			product := int64(1)
			for range argc {
				product *= int64(vm.pop().(*object.Integer).Value)
			}
			vm.push(object.NewInteger(product))
		case code.OpDiv:
			b := vm.pop()
			a := vm.pop()
			if b.(*object.Integer).Value == 0 {
				return errors.New("division by zero")
			}
			res := int64(a.(*object.Integer).Value) / int64(b.(*object.Integer).Value)
			vm.push(object.NewInteger(res))
		case code.OpPop:
			vm.pop()
		}
	}

	return nil
}

func (vm *VM) LastPopped() object.Object {
	return vm.stack[vm.pc]
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
	if vm.pc == 0 {
		return nil
	}
	obj := vm.stack[vm.pc-1]
	vm.pc--
	return obj
}
