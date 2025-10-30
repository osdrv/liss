package object

import (
	"fmt"
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
	Outer *SymbolTable
	Free  []Symbol

	Vars    map[string]Symbol
	NumVars int
}

func NewSymbolTable() *SymbolTable {
	return &SymbolTable{
		Vars:    make(map[string]Symbol),
		Free:    []Symbol{},
		NumVars: 0,
	}
}

func NewNestedSymbolTable(outer *SymbolTable) *SymbolTable {
	return &SymbolTable{
		Outer:   outer,
		Vars:    make(map[string]Symbol),
		Free:    []Symbol{},
		NumVars: 0,
	}
}

func (st *SymbolTable) Define(name string) (Symbol, error) {
	if _, exists := st.Vars[name]; exists {
		return Symbol{}, fmt.Errorf("Symbol is already defined: %s", name)
	}
	symbol := Symbol{Name: name, Index: st.NumVars}
	if st.Outer != nil {
		symbol.Scope = LocalScope
	} else {
		symbol.Scope = GlobalScope
	}
	st.Vars[name] = symbol
	st.NumVars++
	return symbol, nil
}

func (st *SymbolTable) DefineFunctionName(name string) Symbol {
	sym := Symbol{
		Name:  name,
		Index: 0,
		Scope: FunctionScope,
	}
	st.Vars[name] = sym
	return sym
}

func (st *SymbolTable) defineFree(orig Symbol) Symbol {
	st.Free = append(st.Free, orig)
	symbol := Symbol{
		Name:  orig.Name,
		Index: len(st.Free) - 1,
		Scope: FreeScope,
	}
	st.Vars[orig.Name] = symbol
	return symbol
}

func (st *SymbolTable) Resolve(name string) (Symbol, bool) {
	if _, ix, ok := GetBuiltinByName(name); ok {
		return Symbol{Name: name, Index: ix, Scope: BuiltinScope}, true
	}
	symbol, ok := st.Vars[name]
	if !ok && st.Outer != nil {
		symbol, ok = st.Outer.Resolve(name)
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
