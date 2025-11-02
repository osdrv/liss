package object

import (
	"fmt"
	"strings"
)

type Scope uint8

const (
	MODULE_SELF = ""
)

const (
	GlobalScope Scope = iota
	LocalScope
	BuiltinScope
	FreeScope
	FunctionScope
	ModuleScope
)

type Symbol struct {
	Name     string
	ModIndex int
	Index    int
	Scope    Scope
}

type SymbolTable struct {
	Outer *SymbolTable
	Free  []Symbol

	Vars    map[string]Symbol
	NumVars int

	Modules []*Module
	modix   map[string]int
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

func (st *SymbolTable) DefineModule(name string, mod *Module) (int, error) {
	for _, m := range st.Modules {
		if m.Name == name {
			return -1, fmt.Errorf("Module is already defined: %s", name)
		}
	}
	st.Modules = append(st.Modules, mod)
	mix := len(st.Modules) - 1
	if st.modix == nil {
		st.modix = make(map[string]int)
	}
	st.modix[name] = mix
	return mix, nil
}

func (st *SymbolTable) Define(module string, name string) (Symbol, error) {
	if _, exists := st.Vars[name]; exists {
		return Symbol{}, fmt.Errorf("Symbol is already defined: %s", name)
	}
	symbol := Symbol{Name: name, Index: st.NumVars}
	mix := 0
	if module != MODULE_SELF {
		var ok bool
		mix, ok = st.modix[module]
		if !ok {
			return Symbol{}, fmt.Errorf("Module is not defined: %s", module)
		}
		if st.Outer != nil {
			return Symbol{}, fmt.Errorf("Cannot define module-scoped symbol %s in nested scope", name)
		}
	}
	fullName := name
	if module != MODULE_SELF {
		symbol.Scope = ModuleScope
		symbol.ModIndex = mix
		fullName = fmt.Sprintf("%s:%s", module, name)
	} else if st.Outer != nil {
		symbol.Scope = LocalScope
	} else {
		symbol.Scope = GlobalScope
	}
	st.Vars[fullName] = symbol
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

func (st *SymbolTable) Resolve(module string, name string) (Symbol, bool) {
	if _, ix, ok := GetBuiltinByName(name); ok {
		return Symbol{Name: name, Index: ix, Scope: BuiltinScope}, true
	}
	if module != MODULE_SELF {
		modix, ok := st.modix[module]
		if !ok {
			if st.Outer != nil {
				return st.Outer.Resolve(module, name)
			}
			return Symbol{}, false
		}
		sym, ok := st.Modules[modix].Symbols.Resolve(MODULE_SELF, name)
		if !ok {
			return sym, ok
		}
		return Symbol{
			Name:     name,
			ModIndex: modix,
			Index:    sym.Index,
			Scope:    ModuleScope,
		}, true
	}
	symbol, ok := st.Vars[name]
	if !ok && st.Outer != nil {
		symbol, ok = st.Outer.Resolve(module, name)
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
