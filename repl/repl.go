package repl

import (
	"bufio"
	"fmt"
	"io"
	"osdrv/liss/compiler"
	"osdrv/liss/lexer"
	"osdrv/liss/parser"
	"osdrv/liss/vm"
)

const Prompt = "liss> "

func Run(in io.Reader, out io.Writer) {
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
			fmt.Fprintf(out, "Failed to parse program: %s\n", err)
		}
		comp := compiler.New()
		if err := comp.Compile(prog); err != nil {
			fmt.Fprintf(out, "Failed to compile program: %s\n", err)
			continue
		}
		vm := vm.New(comp.Bytecode())
		if err := vm.Run(); err != nil {
			fmt.Fprintf(out, "Failed to run program: %s\n", err)
			continue
		}
		st := vm.StackTop()
		io.WriteString(out, st.String())
		io.WriteString(out, "\n")
	}
}
