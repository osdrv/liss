package vm

import (
	"errors"
	"fmt"
	"os"
	"osdrv/liss/code"
	"osdrv/liss/compiler"
	"osdrv/liss/object"
	"runtime"
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

func NewRuntimeError(msg string) error {
	return fmt.Errorf("runtime error: %s", msg)
}

var True = object.NewBool(true)
var False = object.NewBool(false)
var Null = object.NewNull()

type builtinHook func(*VM, *object.BuiltinFunction, []object.Object) (*object.BuiltinFunction, []object.Object, error)

var appendStdoutHook builtinHook = func(
	vm *VM, fn *object.BuiltinFunction, args []object.Object,
) (*object.BuiltinFunction, []object.Object, error) {
	if len(args) < 1 {
		return nil, nil, fmt.Errorf("print expects at least 1 argument, got %d", len(args))
	}
	if !args[0].IsFile() {
		newargs := make([]object.Object, len(args)+1)
		newargs[0] = vm.files[STDOUT]
		copy(newargs[1:], args)
		args = newargs
	}
	return fn, args, nil
}

var appendDotPathHook builtinHook = func(
	vm *VM, fn *object.BuiltinFunction, args []object.Object,
) (*object.BuiltinFunction, []object.Object, error) {
	if len(args) == 1 {
		// Put current module's dot path as the second argument
		args = append(args, object.NewString(vm.currentFrame().cl.Module.Path))
	}
	return fn, args, nil
}

var (
	DefaultHooks = map[string]builtinHook{
		"print":   appendStdoutHook,
		"println": appendStdoutHook,
		"fopen":   appendDotPathHook,
	}
)

type anchor struct {
	path string
	line int
}

type VMOptions struct {
	Debug int
}

type VM struct {
	stack []object.Object
	sp    int // program counter

	frames   []*Frame
	framesix int

	hooks map[string]builtinHook

	files map[string]*object.File

	opts VMOptions

	lastPopped object.Object
	lastAnchor *anchor
}

func New(mod *compiler.Module) *VM {
	env := object.NewEnvironment()
	env.Globals = make([]object.Object, GlobalsSize)
	env.Consts = mod.Bytecode.Consts

	mainfn := &object.Function{
		Instrs: mod.Bytecode.Instrs,
	}
	cmpmod := &object.Module{
		Name:    mod.Name,
		Path:    mod.Path,
		Instrs:  mod.Bytecode.Instrs,
		Symbols: mod.Symbols,
		Env:     env,
	}
	maincl := object.NewClosure(mainfn, nil, env, cmpmod)
	mframe := NewFrame(maincl, 0)
	frames := make([]*Frame, MaxFrames)
	frames[0] = mframe

	files := make(map[string]*object.File)
	files[STDOUT] = object.NewFile(os.Stdout, STDOUT)
	files[STDERR] = object.NewFile(os.Stderr, STDERR)

	return &VM{
		stack: make([]object.Object, StackSize),
		sp:    0,

		frames:   frames,
		framesix: 1,

		hooks: DefaultHooks,

		files: files,
	}
}

func (vm *VM) Shutdown() {
	for _, f := range vm.files {
		if f != nil && f.Path() != STDOUT && f.Path() != STDERR {
			f.Close()
		}
	}
}

func (vm *VM) WithOptions(opts VMOptions) *VM {
	vm.opts = opts
	return vm
}

func (vm *VM) StackTop() object.Object {
	if vm.sp == 0 {
		return nil
	}
	return vm.stack[vm.sp-1]
}

func (vm *VM) Run() (exit_err error) {
	var ip int
	var instrs code.Instructions
	var op code.OpCode
	defer func() {
		if exit_err != nil && vm.lastAnchor != nil {
			exit_err = fmt.Errorf("%s (at %s:%d)", exit_err.Error(), vm.lastAnchor.path, vm.lastAnchor.line)
		}
	}()

	for vm.currentFrame().ip < len(vm.currentFrame().Instructions())-1 {
		vm.currentFrame().ip++

		ip = vm.currentFrame().ip
		instrs = vm.currentFrame().Instructions()
		env := vm.currentFrame().cl.Env
		op = code.OpCode(instrs[ip])

		if vm.opts.Debug > 1 {
			fmt.Printf("Current frame:\n%s\n", code.PrintInstr(vm.currentFrame().Instructions()))
			fmt.Printf("Curtrent instruction pointer: %d\n", ip)
			fmt.Printf("VM is executing operation: %s (%d)\n", code.PrintOpCode(op), op)
		}

		switch op {
		case code.OpConst:
			ix := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip += 2
			if err := vm.push(env.Consts[ix]); err != nil {
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
			argc := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip += 2
			val := true
			for range int(argc) {
				arg := vm.pop()
				if arg.Type() != object.BoolType {
					return NewTypeMismatchError(
						fmt.Sprintf("expected boolean type, got %s", arg.Type().String()),
					)
				}
				val = val && arg.(*object.Bool).Value
			}
			if err := vm.push(object.NewBool(val)); err != nil {
				return err
			}
		case code.OpOr:
			argc := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip += 2
			val := false
			for range int(argc) {
				arg := vm.pop()
				if arg.Type() != object.BoolType {
					return NewTypeMismatchError(
						fmt.Sprintf("expected boolean type, got %s", arg.Type().String()),
					)
				}
				val = val || arg.(*object.Bool).Value
			}
			if err := vm.push(object.NewBool(val)); err != nil {
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
			env.Globals[gix] = vm.pop()
		case code.OpGetGlobal:
			gix := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip += 2
			if err := vm.push(env.Globals[gix]); err != nil {
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
				for i := range argc {
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
				msg := fmt.Sprintf("Object %s is not a function", vm.stack[vm.sp-1].String())
				if vm.opts.Debug > 0 {
					msg += "\n\n" + fmt.Sprintf("Stack dump:\n%s\n\nClosure:\n  %s\n\nInstructions:\n%s\n\nInstruction pointer: %d\n\nStack trace:\n%s\n\nLocal vars: %s\n",
						vm.PrintStack(),
						vm.currentFrame().cl.String(),
						code.PrintInstr(vm.currentFrame().Instructions()),
						vm.currentFrame().ip,
						vm.PrintStackTrace(),
						vm.PrintLocalVariables(),
					)
				}
				return errors.New(msg)
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
			for i := vm.sp; i >= frame.bptr; i-- {
				vm.stack[i] = nil
			}
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
		case code.OpSrcAnchor:
			line := code.ReadUint16(instrs[ip+1:])
			vm.currentFrame().ip += 2
			vm.lastAnchor = &anchor{path: vm.currentFrame().cl.Module.Path, line: int(line)}
			if vm.opts.Debug > 1 {
				fmt.Printf("Source file: %s\n", vm.currentFrame().cl.Module.Path)
				fmt.Printf("Executing line: %d\n", line)
			}
		case code.OpBreakpoint:
			line := code.ReadUint16(instrs[ip+1:])
			col := code.ReadUint16(instrs[ip+3:])
			vm.currentFrame().ip += 4
			if vm.opts.Debug > 0 {
				fmt.Printf("Breakpoint reached at: line: %d, column: %d\n", line, col)
				runtime.Breakpoint()
			}
		case code.OpRaise:
			errObj := vm.pop()
			return NewRuntimeError(errObj.String())
		case code.OpLoadModule:
			// Nothing at the moment: modules are loaded during compilation
			vm.currentFrame().ip += 2
		case code.OpGetModule:
			modix := code.ReadUint16(instrs[ip+1:])
			globix := code.ReadUint16(instrs[ip+3:])
			vm.currentFrame().ip += 4
			if err := vm.pushModuleSymbol(int(modix), int(globix)); err != nil {
				return err
			}
		default:
			return fmt.Errorf("unknown opcode %s at position %d", code.PrintOpCode(op), ip)
		}
	}

	return nil
}

func (vm *VM) pushModuleSymbol(modix int, constix int) error {
	mod := vm.currentFrame().cl.Env.Consts[modix]
	modObj, ok := mod.(*object.Module)
	if !ok {
		return fmt.Errorf("constant at index %d is not a module", modix)
	}
	item := modObj.Env.Globals[constix]
	switch item := item.(type) {
	case *object.Function:
		closure := object.NewClosure(item, nil, modObj.Env, modObj)
		return vm.push(closure)
	case *object.Closure:
		closure := object.NewClosure(item.Fn, item.Free, modObj.Env, modObj)
		return vm.push(closure)
	default:
		return vm.push(item)
	}
}

func (vm *VM) pushClosure(cix int, numfree int) error {
	env := vm.currentFrame().cl.Env
	cnst := env.Consts[cix]
	fn, ok := cnst.(*object.Function)
	if !ok {
		return fmt.Errorf("constant at index %d is not a function", cix)
	}
	free := make([]object.Object, numfree)
	for i := range numfree {
		free[i] = vm.stack[vm.sp-numfree+i]
	}
	vm.sp = vm.sp - numfree
	closure := object.NewClosure(fn, free, vm.currentFrame().cl.Env, vm.currentFrame().cl.Module)
	return vm.push(closure)
}

func (vm *VM) callFunction(argc int) error {
	if vm.sp-1-argc < 0 {
		runtime.Breakpoint()
	}
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
		for i := vm.sp; i >= vm.sp-argc; i-- {
			vm.stack[i] = nil
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

func (vm *VM) PrintStackTrace() string {
	var b strings.Builder
	for i := vm.framesix - 1; i >= 0; i-- {
		frame := vm.frames[i]
		b.WriteString(fmt.Sprintf("Frame %d: %s (ip=%d)\n", i, frame.cl.Fn.String(), frame.ip))
	}
	return b.String()
}

func (vm *VM) PrintLocalVariables() string {
	var b strings.Builder
	frame := vm.currentFrame()
	b.WriteString("{\n")
	for i := range frame.cl.Fn.NumLocals {
		localVar := vm.stack[frame.bptr+i]
		b.WriteString(fmt.Sprintf("  %d: %s\n", i, localVar.String()))
	}
	b.WriteString("}")
	return b.String()
}

func (vm *VM) Env() *object.Environment {
	return vm.currentFrame().cl.Env
}

// DEPRECATED: use Env().Consts instead
func (vm *VM) Consts() []object.Object {
	return vm.currentFrame().cl.Consts
}

func (vm *VM) LastPopped() object.Object {
	return vm.lastPopped
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
	vm.lastPopped = obj
	vm.stack[vm.sp-1] = nil
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
