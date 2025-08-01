package repl

import (
	"bufio"
	"fmt"
	"io"
	"osdrv/liss/code"
	"osdrv/liss/compiler"
	"osdrv/liss/lexer"
	"osdrv/liss/parser"
	"osdrv/liss/vm"
)

type Options struct {
	Debug bool
}

const Prompt = "liss> "

func Run(in io.Reader, out io.Writer, opts Options) {
	scanner := bufio.NewScanner(in)

	for {
		fmt.Printf(Prompt)
		scanned := scanner.Scan()
		if !scanned {
			return
		}
		line := scanner.Text()
		lex := lexer.NewLexer(line)
		par := parser.NewParser(lex)

		prog, err := par.Parse()
		if err != nil {
			fmt.Printf("Failed to parse program: %s\n", err)
			fmt.Fprintf(out, "Failed to parse program: %s\n", err)
		}
		comp := compiler.New()
		if err := comp.Compile(prog); err != nil {
			fmt.Printf("failed to compile program: %s", err)
			fmt.Fprintf(out, "Failed to compile program: %s\n", err)
			continue
		}
		vm := vm.New(comp.Bytecode())

		if opts.Debug {
			fmt.Printf("VM instructions:\n%s\n", code.PrintInstr(comp.Bytecode().Instrs))
		}

		if err := vm.Run(); err != nil {
			fmt.Fprintf(out, "Failed to run program: %s\n", err)
			continue
		}

		if opts.Debug {
			fmt.Printf("VM stack: %s\n", vm.PrintStack())
		}

		st := vm.LastPopped()
		io.WriteString(out, st.String())
		io.WriteString(out, "\n")
	}
}
