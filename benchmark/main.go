package main

import (
	"log"
	"osdrv/liss/compiler"
	"osdrv/liss/lexer"
	"osdrv/liss/object"
	"osdrv/liss/parser"
	"osdrv/liss/vm"
	"time"
)

var input = `
(fn fib [n]
	(cond (= n 0)
			0
			(cond (= n 1)
					1
					(+ (fib (- n 1)) (fib (- n 2)))
			)
	)
)
(fib 35)
`

func main() {
	var dur time.Duration
	var res object.Object

	lex := lexer.NewLexer(input)
	par := parser.NewParser(lex)
	prog, err := par.Parse()
	if err != nil {
		log.Fatalf("Failed to parse the source code: %s", err)
	}

	comp := compiler.New(compiler.CompilerOptions{})
	if err := comp.Compile(prog); err != nil {
		log.Fatalf("Failed to compile the program: %s", err)
	}

	machine := vm.New(comp.Bytecode())

	start := time.Now()

	if err := machine.Run(); err != nil {
		log.Fatalf("Failed to run the program: %s", err)
	}

	dur = time.Since(start)

	res = machine.LastPopped()

	log.Printf("Result: %s", res.String())
	log.Printf("Execution time: %s", dur)
}
