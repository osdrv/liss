package object

import "osdrv/liss/code"

type Module struct {
	defaultObject
	Name    string
	Path    string
	Instrs  code.Instructions
	Symbols *SymbolTable
	Env     *Environment
}

var _ Object = (*Module)(nil)

func NewModule(name string, path string, instrs code.Instructions,
	symbols *SymbolTable, env *Environment) *Module {
	return &Module{
		Name:    name,
		Path:    path,
		Instrs:  instrs,
		Symbols: symbols,
		Env:     env,
	}
}

func (m *Module) Type() ObjectType {
	return ModuleType
}

func (m *Module) String() string {
	return "<module " + m.Name + ">"
}

func (m *Module) Clone() Object {
	return m
}

func (m *Module) Raw() any {
	return m
}
