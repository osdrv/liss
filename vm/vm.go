package vm

import (
	"errors"
	"osdrv/liss/code"
	"osdrv/liss/compiler"
	"osdrv/liss/object"
	"strings"
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
			elem := vm.peek()
			if elem == nil {
				return errors.New("stack underflow")
			}
			switch elem.Type() {
			case object.IntegerType:
				sum := int64(0)
				for range argc {
					sum += int64(vm.pop().(*object.Integer).Value)
				}
				vm.push(object.NewInteger(sum))
			case object.FloatType:
				sum := float64(0.0)
				for range argc {
					sum += float64(vm.pop().(*object.Float).Value)
				}
				vm.push(object.NewFloat(sum))
			case object.StringType:
				var b strings.Builder
				for range argc {
					str := vm.pop().(*object.String)
					b.WriteString(str.Value)
				}
				vm.push(object.NewString(b.String()))
			default:
				// TODO: it is compiler's job to ensure that the types are correct
				return errors.New("invalid type for addition")
			}
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

func (vm *VM) PrintStack() string {
	var b strings.Builder
	for i := range vm.pc {
		if i > 0 {
			b.WriteString("\n")
		}
		if vm.stack[i] != nil {
			b.WriteString(vm.stack[i].String())
		} else {
			b.WriteString("nil")
		}
	}
	return b.String()
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

func (vm *VM) peek() object.Object {
	if vm.pc == 0 {
		return nil
	}
	return vm.stack[vm.pc-1]
}

func (vm *VM) pop() object.Object {
	if vm.pc == 0 {
		return nil
	}
	obj := vm.stack[vm.pc-1]
	vm.pc--
	return obj
}
