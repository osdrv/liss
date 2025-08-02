package compiler

import "fmt"

type Symbol struct {
	Name  string
	Index int
}

type SymbolTable struct {
	vars    map[string]Symbol
	numVars int
}

func NewSymbolTable() *SymbolTable {
	return &SymbolTable{
		vars:    make(map[string]Symbol),
		numVars: 0,
	}
}

func (st *SymbolTable) Define(name string) (Symbol, error) {
	if _, exists := st.vars[name]; exists {
		return Symbol{}, fmt.Errorf("Symbol is already defined: %s", name)
	}
	symbol := Symbol{Name: name, Index: st.numVars}
	st.vars[name] = symbol
	st.numVars++
	return symbol, nil
}

func (st *SymbolTable) Resolve(name string) (Symbol, bool) {
	symbol, exists := st.vars[name]
	return symbol, exists
}
