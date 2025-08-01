package vm

import (
	"cmp"
	"osdrv/liss/code"
)

var m_int64 = map[code.OpCode]func(a, b int64) bool{
	code.OpEql:          CompareEql[int64],
	code.OpNotEql:       CompareNeq[int64],
	code.OpGreaterThan:  CompareGt[int64],
	code.OpGreaterEqual: CompareGte[int64],
	code.OpLessThan:     CompareLt[int64],
	code.OpLessEqual:    CompareLte[int64],
}

var m_float64 = map[code.OpCode]func(a, b float64) bool{
	code.OpEql:          CompareEql[float64],
	code.OpNotEql:       CompareNeq[float64],
	code.OpGreaterThan:  CompareGt[float64],
	code.OpGreaterEqual: CompareGte[float64],
	code.OpLessThan:     CompareLt[float64],
	code.OpLessEqual:    CompareLte[float64],
}

var m_string = map[code.OpCode]func(a, b string) bool{
	code.OpEql:          CompareEql[string],
	code.OpNotEql:       CompareNeq[string],
	code.OpGreaterThan:  CompareGt[string],
	code.OpGreaterEqual: CompareGte[string],
	code.OpLessThan:     CompareLt[string],
	code.OpLessEqual:    CompareLte[string],
}

var m_bool = map[code.OpCode]func(a, b bool) bool{
	code.OpEql:    CompareEql[bool],
	code.OpNotEql: CompareNeq[bool],
}

func CompareEql[T comparable](a, b T) bool {
	return a == b
}

func CompareNeq[T comparable](a, b T) bool {
	return a != b
}

func CompareLt[T cmp.Ordered](a, b T) bool {
	return a < b
}

func CompareLte[T cmp.Ordered](a, b T) bool {
	return a <= b
}

func CompareGt[T cmp.Ordered](a, b T) bool {
	return a > b
}

func CompareGte[T cmp.Ordered](a, b T) bool {
	return a >= b
}
