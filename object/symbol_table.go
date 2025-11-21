package object

import (
	"fmt"
	"strings"
)

type Scope uint8

const (
	MODULE_UNSET   = ""
	MODULE_SELF    = ""
	MODULE_BUILTIN = "builtin"
)

const LissFileExt = ".liss"

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

func (st *SymbolTable) DefineModule(name string, mod *Module, modix int) error {
	if st.modix == nil {
		st.modix = make(map[string]int)
	}
	if _, ok := st.modix[name]; ok {
		return fmt.Errorf("Module is already defined: %s", name)
	}
	st.Modules = append(st.Modules, mod)
	st.modix[name] = modix
	return nil
}

func (st *SymbolTable) LookupModule(refOrName string) (int, bool) {
	name := refOrName
	if strings.HasSuffix(refOrName, LissFileExt) {
		name = strings.TrimSuffix(refOrName, LissFileExt)
	}
	mix, ok := st.modix[name]
	if !ok {
		// Try looking up by full ref
		for ix, mod := range st.Modules {
			if mod.Path == refOrName {
				return ix, true
			}
		}
	}
	return mix, ok
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

func resolveModuleSymbol(st *SymbolTable, module string, name string) (Symbol, bool) {
	var modix int
	var ok bool

	ptr := st
	for ptr != nil {
		modix, ok = ptr.modix[module]
		if ok {
			if sym, ok := ptr.Modules[modix].Symbols.Resolve(MODULE_SELF, name); ok {
				return Symbol{
					Name:     name,
					ModIndex: modix,
					Index:    sym.Index,
					Scope:    ModuleScope,
				}, true
			}
			break
		}
		ptr = ptr.Outer
	}

	return Symbol{}, false
}

func resolveBuiltinSymbol(st *SymbolTable, module string, name string) (Symbol, bool) {
	if _, ix, ok := GetBuiltinByName(name); ok {
		return Symbol{Name: name, Index: ix, Scope: BuiltinScope}, ok
	}
	return Symbol{}, false
}

func resolveScopeChainSymbol(st *SymbolTable, module string, name string) (Symbol, bool) {
	if symbol, ok := st.Vars[name]; ok {
		return symbol, ok
	}
	if st.Outer != nil {
		symbol, ok := st.Outer.Resolve(module, name)
		if !ok {
			return symbol, ok
		}
		if symbol.Scope == GlobalScope || symbol.Scope == BuiltinScope {
			return symbol, ok
		}
		free := st.defineFree(symbol)
		return free, true
	}
	return Symbol{}, false
}

// Resolution rules
//  1. If module is provided and is not set to Builtin, look up the symbol
//     in the corresponding module only.
//  2. If the module is Builtin, look up the symbol in the builtins only.
//  3. If the module is Self or not provided, look up the symbol in the current scope.
//     a. If not found, look up in the outer scopes recursively.
//     b. If not found in the outer scopes, look up in the builtins.
func (st *SymbolTable) Resolve(module string, name string) (Symbol, bool) {
	if module != MODULE_UNSET && module != MODULE_BUILTIN {
		return resolveModuleSymbol(st, module, name)
	} else if module == MODULE_BUILTIN {
		return resolveBuiltinSymbol(st, module, name)
	}
	if sym, ok := resolveScopeChainSymbol(st, module, name); ok {
		return sym, ok
	}
	return resolveBuiltinSymbol(st, module, name)
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
