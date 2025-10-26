package compiler

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestResolveLocal(t *testing.T) {
	glob := NewSymbolTable()
	glob.Define("a")
	glob.Define("b")

	loc := NewNestedSymbolTable(glob)
	loc.Define("c")
	loc.Define("d")

	want := []Symbol{
		{Name: "a", Scope: GlobalScope, Index: 0},
		{Name: "b", Scope: GlobalScope, Index: 1},
		{Name: "c", Scope: LocalScope, Index: 0},
		{Name: "d", Scope: LocalScope, Index: 1},
	}

	for _, w := range want {
		sym, ok := loc.Resolve(w.Name)
		assert.True(t, ok, "Expected to resolve symbol %q", w.Name)
		assert.Equal(t, w, sym, "Symbols do not match: want: %v, got: %v", w, sym)
	}
}

func TestResolveFree(t *testing.T) {
	glob := NewSymbolTable()
	glob.Define("a")
	glob.Define("b")

	loc1 := NewNestedSymbolTable(glob)
	loc1.Define("c")
	loc1.Define("d")

	loc2 := NewNestedSymbolTable(loc1)
	loc2.Define("e")
	loc2.Define("f")

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
				sym, ok := tt.table.Resolve(w.Name)
				assert.True(t, ok, "Expected to resolve symbol %q", w.Name)
				assert.Equal(t, w, sym, "Symbols do not match: want: %v, got: %v", w, sym)
			}
			for i, w := range tt.wantFree {
				sym, ok := tt.table.Resolve(w.Name)
				assert.True(t, ok, "Expected to resolve symbol %q", w.Name)
				assert.Equal(t, FreeScope, sym.Scope, "Expected symbol %q to be free", w.Name)
				assert.Equal(t, i, sym.Index, "Free symbol %q has wrong index: want: %d, got: %d", w.Name, i, sym.Index)
			}
			assert.Equal(t, tt.wantFree, tt.table.free, "Free symbols do not match: want: %v, got: %v", tt.wantFree, tt.table.free)
		})
	}
}

func TestResolveUnresolvable(t *testing.T) {
	glob := NewSymbolTable()
	glob.Define("a")

	loc1 := NewNestedSymbolTable(glob)
	loc1.Define("b")

	loc2 := NewNestedSymbolTable(loc1)
	loc2.Define("c")
	loc2.Define("d")

	expect := []Symbol{
		{Name: "a", Scope: GlobalScope, Index: 0},
		{Name: "b", Scope: FreeScope, Index: 0},
		{Name: "c", Scope: LocalScope, Index: 0},
		{Name: "d", Scope: LocalScope, Index: 1},
	}

	for _, sym := range expect {
		s, ok := loc2.Resolve(sym.Name)
		assert.True(t, ok, "Expected to resolve symbol %q", sym.Name)
		assert.Equal(t, sym, s, "Symbols do not match: want: %v, got: %v", sym, s)
	}

	unexpected := []string{"e", "f", "g"}
	for _, name := range unexpected {
		_, ok := loc2.Resolve(name)
		assert.False(t, ok, "Did not expect to resolve symbol %q", name)
	}
}

func TestResolveFunctionName(t *testing.T) {
	glob := NewSymbolTable()
	glob.DefineFunctionName("a")

	expect := Symbol{Name: "a", Scope: FunctionScope, Index: 0}
	sym, ok := glob.Resolve("a")
	assert.True(t, ok, "Expected to resolve symbol %q", "a")
	assert.Equal(t, expect, sym, "Symbols do not match: want: %v, got: %v", expect, sym)
}
