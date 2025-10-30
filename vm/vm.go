package vm

import (
	"errors"
	"fmt"
	"os"
	"osdrv/liss/code"
	"osdrv/liss/compiler"
	"osdrv/liss/object"
	"strings"
)

const (
	STDOUT = "STDOUT"
	STDERR = "STDERR"
)

const MaxFrames = 1024
const StackSize = 2048
const GlobalsSize = 65536

var ErrStackOverflow = errors.New("stack overflow")

var True = object.NewBool(true)
var False = object.NewBool(false)
var Null = object.NewNull()

type VMContext struct {
	files map[string]*object.File
}

func NewVMContext() *VMContext {
	return &VMContext{
		files: make(map[string]*object.File),
	}
}

type builtinHook func(*VM, *object.BuiltinFunction, []object.Object) (*object.BuiltinFunction, []object.Object, error)

var appendStdoutHook builtinHook = func(
	vm *VM, fn *object.BuiltinFunction, args []object.Object,
) (*object.BuiltinFunction, []object.Object, error) {
	if len(args) < 1 {
		return nil, nil, fmt.Errorf("print expects at least 1 argument, got %d", len(args))
	}
	if !args[0].IsFile() {
		newargs := make([]object.Object, len(args)+1)
		newargs[0] = vm.ctx.files[STDOUT]
		copy(newargs[1:], args)
		args = newargs
	}
	return fn, args, nil
}

var (
	DefaultHooks = map[string]builtinHook{
		"print":   appendStdoutHook,
		"println": appendStdoutHook,
	}
)

type VM struct {
	consts []object.Object
	//instrs code.Instructions

	stack []object.Object
	sp    int // program counter

	globals []object.Object

	frames   []*Frame
	framesix int

	hooks map[string]builtinHook

	ctx *VMContext
}

func New(bc *compiler.Bytecode) *VM {
	mainfn := &object.Function{
		Instrs: bc.Instrs,
	}
	maincl := object.NewClosure(mainfn, nil)
	mframe := NewFrame(maincl, 0)
	frames := make([]*Frame, MaxFrames)
	frames[0] = mframe

	ctx := NewVMContext()
	ctx.files[STDOUT] = object.NewFile(os.Stdout, STDOUT)
	ctx.files[STDERR] = object.NewFile(os.Stderr, STDERR)

	return &VM{
		consts: bc.Consts,
		stack:  make([]object.Object, StackSize),
		sp:     0,

		globals: make([]object.Object, GlobalsSize),

		frames:   frames,
		framesix: 1,

		hooks: DefaultHooks,

		ctx: ctx,
	}
}

func (vm *VM) Shutdown() {
	for _, f := range vm.ctx.files {
		if f != nil && f.Path() != STDOUT && f.Path() != STDERR {
			f.Close()
		}
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

		// fmt.Printf("Current frame:\n%s\n", code.PrintInstr(vm.currentFrame().Instructions()))
		// fmt.Printf("Curtrent instruction pointer: %d\n", ip)
		// fmt.Printf("VM is executing operation: %d\n", op)

		switch op {
		case code.OpConst:
			//fmt.Printf("Fetching constant\n")
			ix := code.ReadUint16(instrs[ip+1:])
			//fmt.Printf("Read const: %d\n", ix)
			//fmt.Printf("const: %+v\n", vm.consts)
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
					obj := vm.pop()
					if obj.Type() != object.StringType {
						return errors.New("all arguments for string concatenation must be strings")
					}
					str := obj.(*object.String)
					strs = append(strs, str)
				}
				for i := len(strs) - 1; i >= 0; i-- {
					b.WriteString(string(strs[i].Value))
				}
				if err := vm.push(object.NewString(b.String())); err != nil {
					return err
				}
			case object.ListType:
				items := make([]object.Object, 0)
				for range argc {
					obj := vm.pop()
					if obj.Type() != object.ListType {
						return errors.New("all arguments for list addition must be lists")
					}
					listObj := obj.(*object.List)
					items = append(listObj.Items(), items...)
				}
				newList := object.NewList(items)
				if err := vm.push(newList); err != nil {
					return err
				}
			default:
				// TODO: it is compiler's job to ensure that the types are correct
				return errors.New("invalid type for addition")
			}
		case code.OpSub:
			b := vm.pop()
			a := vm.pop()
			var res object.Object
			if a.Type() == object.IntegerType && b.Type() == object.IntegerType {
				sub := a.(object.Numeric).Int64() - b.(object.Numeric).Int64()
				res = object.NewInteger(sub)
			} else {
				af := a.(object.Numeric).Float64()
				bf := b.(object.Numeric).Float64()
				res = object.NewFloat(af - bf)
			}
			if err := vm.push(res); err != nil {
				return err
			}
		case code.OpMul:
			argc := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip += 2

			elem := vm.peek()
			switch elem.Type() {
			case object.IntegerType:
				factor := vm.pop().(*object.Integer).Value
				switch vm.peek().Type() {
				case object.IntegerType:
					product := factor
					for range argc - 1 {
						product *= vm.pop().(*object.Integer).Value
					}
					if err := vm.push(object.NewInteger(product)); err != nil {
						return err
					}
				case object.FloatType:
					product := float64(factor)
					for range argc - 1 {
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
					str := string(vm.pop().(*object.String).Value)
					for i := int64(0); i < factor; i++ {
						b.WriteString(str)
					}
					if err := vm.push(object.NewString(b.String())); err != nil {
						return nil
					}
				case object.ListType:
					if argc != 2 {
						return errors.New("list multiplication requires exactly two arguments")
					}
					listObj := vm.pop().(*object.List)
					items := make([]object.Object, 0, factor*int64(listObj.Len()))
					for i := int64(0); i < factor; i++ {
						items = append(items, listObj.Items()...)
					}
					if err := vm.push(object.NewList(items)); err != nil {
						return err
					}
				default:
					return errors.New("invalid argument type for multiplication")
				}
			case object.FloatType:
				product := float64(1.0)
				for range argc {
					product *= float64(vm.pop().(*object.Float).Value)
				}
				if err := vm.push(object.NewFloat(product)); err != nil {
					return err
				}
			}
		case code.OpDiv:
			b := vm.pop()
			a := vm.pop()
			var res object.Object
			if a.Type() == object.IntegerType && b.Type() == object.IntegerType {
				if b.(*object.Integer).Value == 0 {
					return errors.New("division by zero")
				}
				sub := a.(object.Numeric).Int64() / b.(object.Numeric).Int64()
				res = object.NewInteger(sub)
			} else {
				af := a.(object.Numeric).Float64()
				bf := b.(object.Numeric).Float64()
				res = object.NewFloat(af / bf)
			}
			if err := vm.push(res); err != nil {
				return err
			}
		case code.OpMod:
			b := vm.pop()
			a := vm.pop()
			if a.Type() != object.IntegerType || b.Type() != object.IntegerType {
				return NewTypeMismatchError(
					fmt.Sprintf("expected integer types for modulo operation, got %s and %s", a.Type().String(), b.Type().String()))
			}
			mod := a.(object.Numeric).Int64() % b.(object.Numeric).Int64()
			if err := vm.push(object.NewInteger(mod)); err != nil {
				return err
			}
		case code.OpList:
			size := int(code.ReadUint16(instrs[ip+1:]))
			vm.currentFrame().ip += 2
			items := make([]object.Object, size)
			for i := size - 1; i >= 0; i-- {
				items[i] = vm.pop()
			}
			list := object.NewList(items)
			if err := vm.push(list); err != nil {
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
		case code.OpAnd:
			b := vm.pop()
			a := vm.pop()
			if a.Type() != object.BoolType || b.Type() != object.BoolType {
				return NewTypeMismatchError(
					fmt.Sprintf("expected boolean types, got %s and %s", a.Type().String(), b.Type().String()))
			}
			if err := vm.push(object.NewBool(a.(*object.Bool).Value && b.(*object.Bool).Value)); err != nil {
				return err
			}
		case code.OpOr:
			b := vm.pop()
			a := vm.pop()
			if a.Type() != object.BoolType || b.Type() != object.BoolType {
				return NewTypeMismatchError(
					fmt.Sprintf("expected boolean types, got %s and %s", a.Type().String(), b.Type().String()))
			}
			if err := vm.push(object.NewBool(a.(*object.Bool).Value || b.(*object.Bool).Value)); err != nil {
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
		case code.OpGetBuiltin:
			bix := code.ReadUint8(instrs[ip+1:])
			vm.currentFrame().ip += 1
			bltn, ok := object.GetBuiltinByIndex(int(bix))
			if !ok {
				return fmt.Errorf("builtin function with index %d not found", bix)
			}
			if err := vm.push(bltn); err != nil {
				return err
			}
		case code.OpCall:
			argc := code.ReadUint8(instrs[ip+1:])
			vm.currentFrame().ip += 1
			if err := vm.callFunction(int(argc)); err != nil {
				return err
			}
		case code.OpTailCall:
			argc := int(code.ReadUint8(instrs[ip+1:]))
			vm.currentFrame().ip += 1

			fnobj := vm.stack[vm.sp-1-argc]
			switch fn := fnobj.(type) {
			case *object.Closure:
				if len(fn.Fn.Args) != argc {
					return fmt.Errorf("Function %s expects %d arguments, got %d", fn.Fn.Name, len(fn.Fn.Args), argc)
				}

				for i := 0; i < argc; i++ {
					vm.stack[vm.currentFrame().bptr+i] = vm.stack[vm.sp-argc+i]
				}

				vm.currentFrame().cl = fn
				vm.currentFrame().ip = -1
				vm.sp = vm.currentFrame().bptr + fn.Fn.NumLocals
			case *object.BuiltinFunction:
				args := make([]object.Object, argc)
				for i := range argc {
					args[i] = vm.stack[vm.sp-argc+i]
				}

				if hook, ok := vm.hooks[fn.Name()]; ok {
					var err error
					fn, args, err = hook(vm, fn, args)
					if err != nil {
						return err
					}
				}

				res, err := fn.Invoke(args...)
				if err != nil {
					return err
				}
				vm.sp = vm.sp - argc - 1
				if err := vm.push(res); err != nil {
					return err
				}
			default:
				return fmt.Errorf("Object %s is not a function", vm.stack[vm.sp-1].String())
			}

		case code.OpReturn:
			// Special handling of an empty function body:
			// Example: `((fn []))`: this execution should return null.
			// check if there is anything new on the stack.
			// If not, push null.
			if vm.sp == vm.currentFrame().bptr {
				if err := vm.push(Null); err != nil {
					return err
				}
			}
			ret := vm.pop()
			frame := vm.popFrame()
			vm.sp = frame.bptr - 1
			if err := vm.push(ret); err != nil {
				return err
			}
		case code.OpClosure:
			cix := code.ReadUint16(instrs[ip+1:])
			numfree := code.ReadUint8(instrs[ip+3:]) // number of free variables, not used currently
			vm.currentFrame().ip += 3
			if err := vm.pushClosure(int(cix), int(numfree)); err != nil {
				return err
			}
		case code.OpGetFree:
			fix := code.ReadUint8(instrs[ip+1:])
			vm.currentFrame().ip += 1
			currentClosure := vm.currentFrame().cl
			if err := vm.push(currentClosure.Free[int(fix)]); err != nil {
				return err
			}
		case code.OpCurrentClosure:
			cur := vm.currentFrame().cl
			if err := vm.push(cur); err != nil {
				return err
			}
		default:
			return fmt.Errorf("unknown opcode %s at position %d", code.PrintOpCode(op), ip)
		}
	}

	return nil
}

func (vm *VM) pushClosure(cix int, numfree int) error {
	cnst := vm.consts[cix]
	fn, ok := cnst.(*object.Function)
	if !ok {
		return fmt.Errorf("constant at index %d is not a function", cix)
	}
	free := make([]object.Object, numfree)
	for i := range numfree {
		free[i] = vm.stack[vm.sp-numfree+i]
	}
	vm.sp = vm.sp - numfree
	closure := object.NewClosure(fn, free)
	return vm.push(closure)
}

func (vm *VM) callFunction(argc int) error {
	fnobj := vm.stack[vm.sp-1-argc]
	switch fn := fnobj.(type) {
	case *object.Closure:
		if len(fn.Fn.Args) != argc {
			return fmt.Errorf("Function %s expects %d arguments, got %d", fn.Fn.Name, len(fn.Fn.Args), argc)
		}
		frame := NewFrame(fn, vm.sp-argc)
		vm.pushFrame(frame)
		vm.sp = frame.bptr + fn.Fn.NumLocals
	case *object.BuiltinFunction:
		args := make([]object.Object, argc)
		for i := range argc {
			args[i] = vm.stack[vm.sp-argc+i]
		}

		if hook, ok := vm.hooks[fn.Name()]; ok {
			var err error
			fn, args, err = hook(vm, fn, args)
			if err != nil {
				return err
			}
		}

		res, err := fn.Invoke(args...)
		if err != nil {
			return err
		}
		vm.sp = vm.sp - argc - 1
		if err := vm.push(res); err != nil {
			return err
		}
	default:
		return fmt.Errorf("Object %s is not a function", vm.stack[vm.sp-1].String())
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
