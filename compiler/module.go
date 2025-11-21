package compiler

import "osdrv/liss/object"

type Module struct {
	Name string
	Src  string
	Path string

	Bytecode *Bytecode
	Symbols  *object.SymbolTable
	Env      *object.Environment

	isInitialized bool
}

func NewModule(src string, path string) *Module {
	return &Module{
		Src:     src,
		Path:    path,
		Env:     object.NewEnvironment(),
		Symbols: object.NewSymbolTable(),
	}
}

func (m *Module) IsInitialized() bool {
	return m.isInitialized
}

func (m *Module) MarkInitialized() {
	m.isInitialized = true
}
