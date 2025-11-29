package vm

import (
	"cmp"
	"fmt"
	"osdrv/liss/code"
	"osdrv/liss/object"
)

var m_int64 = []func(a, b int64) bool{
	code.OpEql:          CompareEql[int64],
	code.OpNotEql:       CompareNeq[int64],
	code.OpGreaterThan:  CompareGt[int64],
	code.OpGreaterEqual: CompareGte[int64],
	code.OpLessThan:     CompareLt[int64],
	code.OpLessEqual:    CompareLte[int64],
	code.OpMaxCompre:    nil,
}

var m_float64 = []func(a, b float64) bool{
	code.OpEql:          CompareEql[float64],
	code.OpNotEql:       CompareNeq[float64],
	code.OpGreaterThan:  CompareGt[float64],
	code.OpGreaterEqual: CompareGte[float64],
	code.OpLessThan:     CompareLt[float64],
	code.OpLessEqual:    CompareLte[float64],
	code.OpMaxCompre:    nil,
}

var m_string = []func(a, b string) bool{
	code.OpEql:          CompareEql[string],
	code.OpNotEql:       CompareNeq[string],
	code.OpGreaterThan:  CompareGt[string],
	code.OpGreaterEqual: CompareGte[string],
	code.OpLessThan:     CompareLt[string],
	code.OpLessEqual:    CompareLte[string],
	code.OpMaxCompre:    nil,
}

var m_bool = []func(a, b bool) bool{
	code.OpEql:       CompareEql[bool],
	code.OpNotEql:    CompareNeq[bool],
	code.OpMaxCompre: nil,
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

func compare(a, b object.Object, cmp code.OpCode) (bool, error) {
	if a.IsNull() || b.IsNull() {
		if cmp == code.OpEql {
			return a.IsNull() && b.IsNull(), nil
		}
		if cmp == code.OpNotEql {
			return !a.IsNull() || !b.IsNull(), nil
		}
		return false, NewUnsupportedOpTypeError(
			fmt.Sprintf("unsupported operation %s for null type", code.PrintOpCode(cmp)))
	}

	ac, bc := castTypes(a, b, cmp)
	if ac.Type() != bc.Type() {
		return false, NewTypeMismatchError(
			fmt.Sprintf("%s Vs %s with comparator %s", ac.Type().String(), bc.Type().String(), code.PrintOpCode(cmp)))
	}

	switch ac.Type() {
	case object.IntegerType:
		cmpfn := m_int64[cmp]
		if cmpfn == nil {
			return false, NewUnsupportedOpTypeError(
				fmt.Sprintf("unsupported operation %s for type %s", code.PrintOpCode(cmp), ac.Type().String()))
		}
		return cmpfn(ac.(*object.Integer).Value, bc.(*object.Integer).Value), nil
	case object.FloatType:
		cmpfn := m_float64[cmp]
		if cmpfn == nil {
			return false, NewUnsupportedOpTypeError(
				fmt.Sprintf("unsupported operation %v for type %s", code.PrintOpCode(cmp), ac.Type().String()))
		}
		return cmpfn(ac.(*object.Float).Value, bc.(*object.Float).Value), nil
	case object.StringType:
		cmpfn := m_string[cmp]
		if cmpfn == nil {
			return false, NewUnsupportedOpTypeError(
				fmt.Sprintf("unsupported operation %v for type %s", code.PrintOpCode(cmp), ac.Type().String()))
		}
		return cmpfn(string(ac.(*object.String).Value), string(bc.(*object.String).Value)), nil
	case object.BoolType:
		cmpfn := m_bool[cmp]
		if cmpfn == nil {
			return false, NewUnsupportedOpTypeError(
				fmt.Sprintf("unsupported operation %v for type %s", code.PrintOpCode(cmp), ac.Type().String()))
		}
		return cmpfn(ac.(*object.Bool).Value, bc.(*object.Bool).Value), nil
	default:
		return false, NewUnsupportedOpTypeError(
			fmt.Sprintf("unsupported type %s for comparison", ac.Type().String()))
	}
}
