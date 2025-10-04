package vm

import (
	"errors"
	"fmt"
	"osdrv/liss/code"
	"osdrv/liss/compiler"
	"osdrv/liss/object"
	"strings"
)

const MaxFrames = 1024
const StackSize = 2048
const GlobalsSize = 65536

var ErrStackOverflow = errors.New("stack overflow")

var True = object.NewBool(true)
var False = object.NewBool(false)
var Null = object.NewNull()

type VM struct {
	consts []object.Object
	//instrs code.Instructions

	stack []object.Object
	sp    int // program counter

	globals []object.Object

	frames   []*Frame
	framesix int
}

func New(bc *compiler.Bytecode) *VM {

	mainfn := &object.Function{
		Instrs: bc.Instrs,
	}
	mframe := NewFrame(mainfn, 0)
	frames := make([]*Frame, MaxFrames)
	frames[0] = mframe

	return &VM{
		consts: bc.Consts,
		stack:  make([]object.Object, StackSize),
		sp:     0,

		globals: make([]object.Object, GlobalsSize),

		frames:   frames,
		framesix: 1,
	}
}

func NewWithGlobals(bc *compiler.Bytecode, globals []object.Object) *VM {
	vm := New(bc)
	vm.globals = globals
	return vm
}

func (vm *VM) StackTop() object.Object {
	if vm.sp == 0 {
		return nil
	}
	return vm.stack[vm.sp-1]
}

func (vm *VM) Run() error {
	var ip int
	var instrs code.Instructions
	var op code.OpCode

	for vm.currentFrame().ip < len(vm.currentFrame().Instructions())-1 {
		vm.currentFrame().ip++

		ip = vm.currentFrame().ip
		instrs = vm.currentFrame().Instructions()
		op = code.OpCode(instrs[ip])

		fmt.Printf("Current frame:\n%s\n", code.PrintInstr(vm.currentFrame().Instructions()))
		fmt.Printf("Curtrent instruction pointer: %d\n", ip)
		fmt.Printf("VM is executing operation: %d\n", op)

		switch op {
		case code.OpConst:
			fmt.Printf("Fetching constant\n")
			ix := code.ReadUint16(instrs[ip+1:])
			fmt.Printf("Read const: %d\n", ix)
			fmt.Printf("const: %+v\n", vm.consts)
			vm.currentFrame().ip += 2
			if err := vm.push(vm.consts[ix]); err != nil {
				return err
			}
		case code.OpAdd:
			argc := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip += 2
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
				if err := vm.push(object.NewInteger(sum)); err != nil {
					return err
				}
			case object.FloatType:
				sum := float64(0.0)
				for range argc {
					sum += float64(vm.pop().(*object.Float).Value)
				}
				if err := vm.push(object.NewFloat(sum)); err != nil {
					return err
				}
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
				if err := vm.push(object.NewString(b.String())); err != nil {
					return err
				}
			default:
				// TODO: it is compiler's job to ensure that the types are correct
				return errors.New("invalid type for addition")
			}
		case code.OpSub:
			b := vm.pop()
			a := vm.pop()
			res := int64(a.(*object.Integer).Value) - int64(b.(*object.Integer).Value)
			if err := vm.push(object.NewInteger(res)); err != nil {
				return err
			}
		case code.OpMul:
			argc := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip += 2

			elem := vm.peek()
			switch elem.Type() {
			case object.IntegerType:
				product := int64(1)
				for range argc {
					product *= int64(vm.pop().(*object.Integer).Value)
				}
				if err := vm.push(object.NewInteger(product)); err != nil {
					return err
				}
			case object.FloatType:
				product := float64(1.0)
				for range argc {
					product *= float64(vm.pop().(*object.Float).Value)
				}
				if err := vm.push(object.NewFloat(product)); err != nil {
					return err
				}
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
				if err := vm.push(object.NewString(b.String())); err != nil {
					return nil
				}
			}
		case code.OpDiv:
			b := vm.pop()
			a := vm.pop()
			if b.(*object.Integer).Value == 0 {
				return errors.New("division by zero")
			}
			res := int64(a.(*object.Integer).Value) / int64(b.(*object.Integer).Value)
			if err := vm.push(object.NewInteger(res)); err != nil {
				return err
			}
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
			if err := vm.push(object.NewBool(cmp)); err != nil {
				return err
			}
		case code.OpNot:
			a := vm.pop()
			if a.Type() != object.BoolType {
				return NewTypeMismatchError(
					fmt.Sprintf("expected boolean type, got %s", a.Type().String()))
			}
			if err := vm.push(object.NewBool(!a.(*object.Bool).Value)); err != nil {
				return err
			}
		case code.OpTrue:
			if err := vm.push(True); err != nil {
				return err
			}
		case code.OpFalse:
			if err := vm.push(False); err != nil {
				return err
			}
		case code.OpNull:
			if err := vm.push(Null); err != nil {
				return err
			}
		case code.OpPop:
			vm.pop()
		case code.OpJump:
			pos := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip = int(pos) - 1
		case code.OpJumpIfFalse:
			pos := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip += 2
			cond := vm.pop()
			if cond.IsNull() || (cond.Type() == object.BoolType && !cond.(*object.Bool).Value) {
				vm.currentFrame().ip = int(pos) - 1 // -1 because we will increment ip at the end of the loop
			}
		case code.OpSetGlobal:
			gix := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip += 2
			vm.globals[gix] = vm.pop()
		case code.OpGetGlobal:
			gix := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip += 2
			if err := vm.push(vm.globals[gix]); err != nil {
				return err
			}
		case code.OpSetLocal:
			lix := code.ReadUint8(instrs[ip+1:])
			vm.currentFrame().ip += 1
			frame := vm.currentFrame()
			vm.stack[frame.bptr+int(lix)] = vm.pop()
		case code.OpGetLocal:
			lix := code.ReadUint8(instrs[ip+1:])
			vm.currentFrame().ip += 1
			frame := vm.currentFrame()
			if err := vm.push(vm.stack[frame.bptr+int(lix)]); err != nil {
				return err
			}
		case code.OpCall:
			fn, ok := vm.stack[vm.sp-1].(*object.Function)
			if !ok {
				return fmt.Errorf("Object %s is not a function", vm.stack[vm.sp-1].String())
			}
			frame := NewFrame(fn, vm.sp)
			vm.pushFrame(frame)
			vm.sp = frame.bptr + fn.NumLocals
		case code.OpReturn:
			ret := vm.pop()
			frame := vm.popFrame()
			vm.sp = frame.bptr - 1
			if err := vm.push(ret); err != nil {
				return err
			}
		default:
			return fmt.Errorf("unknown opcode %s at position %d", code.PrintOpCode(op), ip)
		}
	}

	return nil
}

func (vm *VM) PrintStack() string {
	var b strings.Builder
	b.WriteByte('[')
	for i := range vm.sp {
		if i > 0 {
			b.WriteString("\n")
		}
		if vm.stack[i] != nil {
			b.WriteString(vm.stack[i].String())
		} else {
			b.WriteString("nil")
		}
	}
	b.WriteByte(']')
	return b.String()
}

func (vm *VM) Consts() []object.Object {
	return vm.consts
}

func (vm *VM) LastPopped() object.Object {
	return vm.stack[vm.sp]
}

func (vm *VM) push(obj object.Object) error {
	if vm.sp >= StackSize {
		return ErrStackOverflow
	}
	vm.stack[vm.sp] = obj
	vm.sp++

	return nil
}

func (vm *VM) peek() object.Object {
	if vm.sp == 0 {
		return nil
	}
	return vm.stack[vm.sp-1]
}

func (vm *VM) pop() object.Object {
	if vm.sp == 0 {
		return nil
	}
	obj := vm.stack[vm.sp-1]
	vm.sp--
	return obj
}

func (vm *VM) currentFrame() *Frame {
	return vm.frames[vm.framesix-1]
}

func (vm *VM) pushFrame(f *Frame) {
	vm.frames[vm.framesix] = f
	vm.framesix++
}
func (vm *VM) popFrame() *Frame {
	vm.framesix--
	return vm.frames[vm.framesix]
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
