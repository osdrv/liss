package main

import (
	"flag"
	"fmt"
	"os"
	"osdrv/liss/compiler"
	"osdrv/liss/lexer"
	"osdrv/liss/parser"
	"osdrv/liss/repl"
	"osdrv/liss/vm"
	"strings"
)

type Result interface {
	String() string
}

func Run(bc *compiler.Bytecode, opts repl.Options) (Result, error) {
	vm := vm.New(bc)
	defer vm.Shutdown()
	if err := vm.Run(); err != nil {
		return nil, fmt.Errorf("failed to run bytecode: %w", err)
	}
	top := vm.LastPopped()
	if opts.Debug {
		fmt.Printf("VM stack: %s\n", vm.PrintStack())
	}
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

func Execute(src string, opts repl.Options) (Result, error) {
	bytecode, err := Compile(src)
	if err != nil {
		return nil, err
	}
	return Run(bytecode, opts)
}

func main() {
	out := os.Stdout
	er := os.Stderr
	defer out.Close()
	defer er.Close()

	debug := flag.Bool("debug", false, "Enables debug mode with stack dumps and traces")
	src := flag.String("src", "", "Source file to execute")
	exec := flag.String("exec", "", "Execute source code and exit")
	flag.Parse()
	opts := repl.Options{
		Debug:       *debug,
		Interactive: *exec == "",
	}
	if opts.Debug {
		fmt.Printf("Runtime flags: %+v\n", opts)
	}

	if *exec != "" {
		opts.Interactive = false
		in := strings.NewReader(strings.TrimSpace(*exec) + "\n")
		repl.Run(in, out, opts)
		os.Exit(0)
	}

	if *src == "" {
		in := os.Stdin
		defer in.Close()
		repl.Run(in, out, opts)
		os.Exit(0)
	}

	data, err := os.ReadFile(*src)
	if err != nil {
		fmt.Fprintf(er, "Error reading file %s: %v\n", *src, err)
		os.Exit(1)
	}
	if _, err := Execute(string(data), opts); err != nil {
		fmt.Fprintf(er, "Error executing file %s: %v\n", *src, err)
		os.Exit(1)
	}

	os.Exit(0)
}
