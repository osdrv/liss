package repl

import (
	"bufio"
	"fmt"
	"io"
	"osdrv/liss/code"
	"osdrv/liss/compiler"
	"osdrv/liss/lexer"
	"osdrv/liss/object"
	"osdrv/liss/parser"
	"osdrv/liss/vm"
)

type Options struct {
	Debug       bool
	Interactive bool
	Verbose     bool
}

const Prompt = "liss> "

func Run(in io.Reader, out io.Writer, opts Options) {
	scanner := bufio.NewScanner(in)
	consts := []object.Object{}
	globs := make([]object.Object, vm.GlobalsSize)
	symbols := object.NewSymbolTable()

	defer func() {
		if err := recover(); err != nil {
			fmt.Fprintf(out, "REPL panicked: %s\n", err)
		}
	}()

	for {
		if opts.Interactive {
			fmt.Printf(Prompt)
		}
		scanned := scanner.Scan()
		if !scanned {
			return
		}
		line := scanner.Text()
		lex := lexer.NewLexer(line)
		par := parser.NewParser(lex)

		prog, err := par.Parse()
		if err != nil {
			fmt.Fprintf(out, "Failed to parse program: %s\n", err)
		}
		if opts.Debug {
			fmt.Printf("AST:\n%s\n", prog.String())
			fmt.Printf("Pre-comp consts: %+v\n", consts)
		}
		comp := compiler.NewWithState(compiler.CompilerOptions{
			Debug: opts.Debug,
		}, symbols, consts)
		if err := comp.Compile(prog); err != nil {
			fmt.Fprintf(out, "Failed to compile program: %s\n", err)
			continue
		}

		mod := &compiler.Module{
			Name:     "repl",
			Bytecode: comp.Bytecode(),
			Symbols:  symbols,
			Env: &object.Environment{
				Globals: globs,
				Consts:  comp.Bytecode().Consts,
			},
		}
		vminst := vm.New(mod)
		if opts.Debug {
			vminst = vminst.WithOptions(vm.VMOptions{Debug: 1})
		}

		if opts.Debug {
			fmt.Printf("VM instructions:\n%s\n", code.PrintInstr(comp.Bytecode().Instrs))
		}

		if err := vminst.Run(); err != nil {
			fmt.Fprintf(out, "Failed to run program: %s\n", err)
			continue
		}

		if opts.Debug {
			fmt.Printf("VM stack: %s\n", vminst.PrintStack())
		}

		consts = vminst.Env().Consts
		// consts = vminst.Consts()
		if opts.Debug {
			fmt.Printf("VM Constants: %+v\n", consts)
		}

		st := vminst.LastPopped()
		io.WriteString(out, st.String())
		io.WriteString(out, "\n")
	}
}
