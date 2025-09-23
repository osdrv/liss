package compiler

import "fmt"

type Scope uint8

const (
	GlobalScope Scope = iota
	LocalScope
)

type Symbol struct {
	Name  string
	Index int
	Scope Scope
}

type SymbolTable struct {
	outer *SymbolTable

	vars    map[string]Symbol
	numVars int
}

func NewSymbolTable() *SymbolTable {
	return &SymbolTable{
		vars:    make(map[string]Symbol),
		numVars: 0,
	}
}

func NewNestedSymbolTable(outer *SymbolTable) *SymbolTable {
	return &SymbolTable{
		outer:   outer,
		vars:    make(map[string]Symbol),
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

func (st *SymbolTable) Resolve(name string) (Symbol, bool) {
	symbol, exists := st.vars[name]
	if !exists && st.outer != nil {
		return st.outer.Resolve(name)
	}
	return symbol, exists
}
