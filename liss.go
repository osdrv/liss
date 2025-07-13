package main

type Result any

type Bytecode []byte

func Run(bytecode Bytecode) (Result, error) {
	panic("Not implemented yet")
}

func Compile(src string) (Bytecode, error) {
	panic("Not implemented yet")
}

func Execute(src string) (Result, error) {
	bytecode, err := Compile(src)
	if err != nil {
		return nil, err
	}
	return Run(bytecode)
}
