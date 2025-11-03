package object

import "osdrv/liss/code"

type Module struct {
	defaultObject
	Name    string
	Instrs  code.Instructions
	Symbols *SymbolTable
	// DEPRECATED: use Env.Consts instead
	Consts []Object
	Env    *Environment
}

var _ Object = (*Module)(nil)

func NewModule(name string, instrs code.Instructions,
	symbols *SymbolTable, consts []Object, env *Environment) *Module {
	return &Module{
		Name:    name,
		Instrs:  instrs,
		Symbols: symbols,
		Consts:  consts,
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
