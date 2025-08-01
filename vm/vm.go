package vm

import (
	"errors"
	"fmt"
	"osdrv/liss/code"
	"osdrv/liss/compiler"
	"osdrv/liss/object"
	"strings"
)

const StackSize = 2048

var ErrStackOverflow = errors.New("stack overflow")

var True = object.NewBool(true)
var False = object.NewBool(false)
var Null = object.NewNull()

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
				strs := make([]*object.String, 0, argc)
				for range argc {
					str := vm.pop().(*object.String)
					strs = append(strs, str)
				}
				for i := len(strs) - 1; i >= 0; i-- {
					b.WriteString(strs[i].Value)
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

			elem := vm.peek()
			switch elem.Type() {
			case object.IntegerType:
				product := int64(1)
				for range argc {
					product *= int64(vm.pop().(*object.Integer).Value)
				}
				vm.push(object.NewInteger(product))
			case object.FloatType:
				product := float64(1.0)
				for range argc {
					product *= float64(vm.pop().(*object.Float).Value)
				}
				vm.push(object.NewFloat(product))
			case object.StringType:
				if argc != 2 {
					return errors.New("string multiplication requires exactly two arguments")
				}
				var b strings.Builder
				str := vm.pop().(*object.String).Value
				reps := vm.pop()
				if reps.Type() != object.IntegerType {
					return errors.New("invalid argument type for string multiplication")
				}
				for i := int64(0); i < int64(reps.(*object.Integer).Value); i++ {
					b.WriteString(str)
				}
				vm.push(object.NewString(b.String()))
			}
		case code.OpDiv:
			b := vm.pop()
			a := vm.pop()
			if b.(*object.Integer).Value == 0 {
				return errors.New("division by zero")
			}
			res := int64(a.(*object.Integer).Value) / int64(b.(*object.Integer).Value)
			vm.push(object.NewInteger(res))
		case code.OpEql,
			code.OpNotEql,
			code.OpGreaterThan,
			code.OpGreaterEqual,
			code.OpLessThan,
			code.OpLessEqual:
			b := vm.pop()
			a := vm.pop()
			cmp, err := compare(a, b, op)
			if err != nil {
				return err
			}
			vm.push(object.NewBool(cmp))
		case code.OpNot:
			a := vm.pop()
			if a.Type() != object.BoolType {
				return NewTypeMismatchError(
					fmt.Sprintf("expected boolean type, got %s", a.Type().String()))
			}
			vm.push(object.NewBool(!a.(*object.Bool).Value))
		case code.OpTrue:
			vm.push(True)
		case code.OpFalse:
			vm.push(False)
		case code.OpNull:
			vm.push(Null)
		case code.OpPop:
			vm.pop()
		default:
			return fmt.Errorf("unknown opcode %s at position %d", code.PrintOpCode(op), ip)
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

func castTypes(a, b object.Object, cmp code.OpCode) (object.Object, object.Object) {
	if a.Type() == b.Type() {
		return a, b
	}

	if a.IsNumeric() && b.IsNumeric() {
		af := a.(object.Numeric).Float64()
		bf := b.(object.Numeric).Float64()
		// TODO: we only need to cast one of them, but we do both for simplicity
		return object.NewFloat(af), object.NewFloat(bf)
	}

	return a, b
}
