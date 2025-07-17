package main

import (
	"fmt"
	"io"
	"os"
	"osdrv/liss/compiler"
	"osdrv/liss/lexer"
	"osdrv/liss/parser"
	"osdrv/liss/repl"
	"osdrv/liss/vm"
)

type Result interface {
	String() string
}

func Run(bc *compiler.Bytecode) (Result, error) {
	vm := vm.New(bc)
	if err := vm.Run(); err != nil {
		return nil, fmt.Errorf("failed to run bytecode: %w", err)
	}
	top := vm.StackTop()
	return top, nil
}

func Compile(src string) (*compiler.Bytecode, error) {
	lex := lexer.NewLexer(src)
	par := parser.NewParser(lex)
	prog, err := par.Parse()
	if err != nil {
		return nil, fmt.Errorf("failed to parse program: %w", err)
	}
	c := compiler.New()
	if err := c.Compile(prog); err != nil {
		return nil, fmt.Errorf("failed to compile program: %w", err)
	}
	return c.Bytecode(), nil
}

func Execute(src string) (Result, error) {
	bytecode, err := Compile(src)
	if err != nil {
		return nil, err
	}
	return Run(bytecode)
}

func main() {
	args := os.Args[1:]
	out := os.Stdout
	er := os.Stderr
	defer out.Close()
	defer er.Close()

	if len(args) == 0 {
		in := os.Stdin
		defer in.Close()
		repl.Run(in, out)
		os.Exit(0)
	}
	src := args[0]
	data, err := os.ReadFile(src)
	if err != nil {
		fmt.Fprintf(er, "Error reading file %s: %v\n", src, err)
		os.Exit(1)
	}
	result, err := Execute(string(data))
	if err != nil {
		fmt.Fprintf(er, "Error executing file %s: %v\n", src, err)
		os.Exit(1)
	}
	io.WriteString(out, result.String()+"\n")

	os.Exit(0)
}
