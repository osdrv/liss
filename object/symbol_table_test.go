package object

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestResolveLocal(t *testing.T) {
	glob := NewSymbolTable()
	glob.Define(MODULE_SELF, "a")
	glob.Define(MODULE_SELF, "b")

	loc := NewNestedSymbolTable(glob)
	loc.Define(MODULE_SELF, "c")
	loc.Define(MODULE_SELF, "d")

	want := []Symbol{
		{Name: "a", Scope: GlobalScope, Index: 0},
		{Name: "b", Scope: GlobalScope, Index: 1},
		{Name: "c", Scope: LocalScope, Index: 0},
		{Name: "d", Scope: LocalScope, Index: 1},
	}

	for _, w := range want {
		sym, ok := loc.Resolve(MODULE_SELF, w.Name)
		assert.True(t, ok, "Expected to resolve symbol %q", w.Name)
		assert.Equal(t, w, sym, "Symbols do not match: want: %v, got: %v", w, sym)
	}
}

func TestResolveFree(t *testing.T) {
	glob := NewSymbolTable()
	glob.Define(MODULE_SELF, "a")
	glob.Define(MODULE_SELF, "b")

	loc1 := NewNestedSymbolTable(glob)
	loc1.Define(MODULE_SELF, "c")
	loc1.Define(MODULE_SELF, "d")

	loc2 := NewNestedSymbolTable(loc1)
	loc2.Define(MODULE_SELF, "e")
	loc2.Define(MODULE_SELF, "f")

	tests := []struct {
		name        string
		table       *SymbolTable
		wantSymbols []Symbol
		wantFree    []Symbol
	}{
		{
			name:  "Resolve first local",
			table: loc1,
			wantSymbols: []Symbol{
				{Name: "a", Scope: GlobalScope, Index: 0},
				{Name: "b", Scope: GlobalScope, Index: 1},
				{Name: "c", Scope: LocalScope, Index: 0},
				{Name: "d", Scope: LocalScope, Index: 1},
			},
			wantFree: []Symbol{},
		},
		{
			name:  "Resolve second local with free vars",
			table: loc2,
			wantSymbols: []Symbol{
				{Name: "a", Scope: GlobalScope, Index: 0},
				{Name: "b", Scope: GlobalScope, Index: 1},
				{Name: "c", Scope: FreeScope, Index: 0},
				{Name: "d", Scope: FreeScope, Index: 1},
				{Name: "e", Scope: LocalScope, Index: 0},
				{Name: "f", Scope: LocalScope, Index: 1},
			},
			wantFree: []Symbol{
				{Name: "c", Scope: LocalScope, Index: 0},
				{Name: "d", Scope: LocalScope, Index: 1},
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			for _, w := range tt.wantSymbols {
				sym, ok := tt.table.Resolve(MODULE_SELF, w.Name)
				assert.True(t, ok, "Expected to resolve symbol %q", w.Name)
				assert.Equal(t, w, sym, "Symbols do not match: want: %v, got: %v", w, sym)
			}
			for i, w := range tt.wantFree {
				sym, ok := tt.table.Resolve(MODULE_SELF, w.Name)
				assert.True(t, ok, "Expected to resolve symbol %q", w.Name)
				assert.Equal(t, FreeScope, sym.Scope, "Expected symbol %q to be free", w.Name)
				assert.Equal(t, i, sym.Index,
					"Free symbol %q has wrong index: want: %d, got: %d", w.Name, i, sym.Index)
			}
			assert.Equal(t, tt.wantFree, tt.table.Free,
				"Free symbols do not match: want: %v, got: %v", tt.wantFree, tt.table.Free)
		})
	}
}

func TestResolveUnresolvable(t *testing.T) {
	glob := NewSymbolTable()
	glob.Define(MODULE_SELF, "a")

	loc1 := NewNestedSymbolTable(glob)
	loc1.Define(MODULE_SELF, "b")

	loc2 := NewNestedSymbolTable(loc1)
	loc2.Define(MODULE_SELF, "c")
	loc2.Define(MODULE_SELF, "d")

	expect := []Symbol{
		{Name: "a", Scope: GlobalScope, Index: 0},
		{Name: "b", Scope: FreeScope, Index: 0},
		{Name: "c", Scope: LocalScope, Index: 0},
		{Name: "d", Scope: LocalScope, Index: 1},
	}

	for _, sym := range expect {
		s, ok := loc2.Resolve(MODULE_SELF, sym.Name)
		assert.True(t, ok, "Expected to resolve symbol %q", sym.Name)
		assert.Equal(t, sym, s, "Symbols do not match: want: %v, got: %v", sym, s)
	}

	unexpected := []string{"e", "f", "g"}
	for _, name := range unexpected {
		_, ok := loc2.Resolve("", name)
		assert.False(t, ok, "Did not expect to resolve symbol %q", name)
	}
}

func TestResolveFunctionName(t *testing.T) {
	glob := NewSymbolTable()
	glob.DefineFunctionName("a")

	expect := Symbol{Name: "a", Scope: FunctionScope, Index: 0}
	sym, ok := glob.Resolve(MODULE_SELF, "a")
	assert.True(t, ok, "Expected to resolve symbol %q", "a")
	assert.Equal(t, expect, sym, "Symbols do not match: want: %v, got: %v", expect, sym)
}
