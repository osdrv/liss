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
