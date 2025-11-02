package object

import "osdrv/liss/code"

type Module struct {
	defaultObject
	Name    string
	Instrs  code.Instructions
	Symbols *SymbolTable
	Consts  []Object
}

var _ Object = (*Module)(nil)

func NewModule(name string, instrs code.Instructions,
	symbols *SymbolTable, consts []Object) *Module {
	return &Module{
		Name:    name,
		Instrs:  instrs,
		Symbols: symbols,
		Consts:  consts,
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
