package object

import (
	"fmt"
	"strings"
)

type Scope uint8

const (
	GlobalScope Scope = iota
	LocalScope
	BuiltinScope
	FreeScope
	FunctionScope
	ModuleScope
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

	Modules map[string]*SymbolTable
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

func (st *SymbolTable) Export(list []string) []Symbol {
	wantAll := len(list) == 0
	wantSet := make(map[string]bool)
	for _, name := range list {
		wantSet[name] = true
	}
	exp := []Symbol{}
	for _, sym := range st.Vars {
		if !isPublicSymbol(sym) {
			continue
		}
		if wantAll || wantSet[sym.Name] {
			exp = append(exp, sym)
		}
	}
	return exp
}

func isPublicSymbol(sym Symbol) bool {
	// We import all global symbols that do not start with an underscore
	return sym.Scope == GlobalScope && !strings.HasPrefix(sym.Name, "_")
}
