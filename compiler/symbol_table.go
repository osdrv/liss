package compiler

import (
	"fmt"
	"osdrv/liss/object"
)

type Scope uint8

const (
	GlobalScope Scope = iota
	LocalScope
	BuiltinScope
	FreeScope
	FunctionScope
)

type Symbol struct {
	Name  string
	Index int
	Scope Scope
}

type SymbolTable struct {
	outer *SymbolTable
	free  []Symbol

	vars    map[string]Symbol
	numVars int
}

func NewSymbolTable() *SymbolTable {
	return &SymbolTable{
		vars:    make(map[string]Symbol),
		free:    []Symbol{},
		numVars: 0,
	}
}

func NewNestedSymbolTable(outer *SymbolTable) *SymbolTable {
	return &SymbolTable{
		outer:   outer,
		vars:    make(map[string]Symbol),
		free:    []Symbol{},
		numVars: 0,
	}
}

func (st *SymbolTable) Define(name string) (Symbol, error) {
	if _, exists := st.vars[name]; exists {
		return Symbol{}, fmt.Errorf("Symbol is already defined: %s", name)
	}
	symbol := Symbol{Name: name, Index: st.numVars}
	if st.outer != nil {
		symbol.Scope = LocalScope
	} else {
		symbol.Scope = GlobalScope
	}
	st.vars[name] = symbol
	st.numVars++
	return symbol, nil
}

func (st *SymbolTable) DefineFunctionName(name string) Symbol {
	sym := Symbol{
		Name:  name,
		Index: 0,
		Scope: FunctionScope,
	}
	st.vars[name] = sym
	return sym
}

func (st *SymbolTable) defineFree(orig Symbol) Symbol {
	st.free = append(st.free, orig)
	symbol := Symbol{
		Name:  orig.Name,
		Index: len(st.free) - 1,
		Scope: FreeScope,
	}
	st.vars[orig.Name] = symbol
	return symbol
}

func (st *SymbolTable) Resolve(name string) (Symbol, bool) {
	if _, ix, ok := object.GetBuiltinByName(name); ok {
		return Symbol{Name: name, Index: ix, Scope: BuiltinScope}, true
	}
	symbol, ok := st.vars[name]
	if !ok && st.outer != nil {
		symbol, ok = st.outer.Resolve(name)
		if !ok {
			return symbol, ok
		}
		if symbol.Scope == GlobalScope || symbol.Scope == BuiltinScope {
			return symbol, ok
		}
		free := st.defineFree(symbol)
		return free, true
	}
	return symbol, ok
}
