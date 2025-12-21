package test

import (
	"osdrv/liss/object"
	"testing"

	"github.com/stretchr/testify/assert"
)

func AssertObjectEql(t *testing.T, got object.Object, want any) {
	t.Helper()
	if got == nil {
		assert.Equal(t, want, got, "Expected object to be %v, got %v", want, got)
		return
	}
	switch obj := got.(type) {
	case *object.Integer:
		assert.Equal(t, want, obj.Raw(), "Expected integer value %v, got %v", want, obj.Value)
	case *object.Float:
		assert.Equal(t, want, obj.Raw(), "Expected float value %v, got %v", want, obj.Value)
	case *object.String:
		assert.Equal(t, want, obj.Raw(), "Expected string value %v, got %v", want, obj.Value)
	case *object.Null:
		assert.Equal(t, want, nil, "Expected null object, got %v", obj)
	case *object.Bool:
		assert.Equal(t, want, obj.Raw(), "Expected boolean value %v, got %v", want, obj.Value)
	case *object.List:
		wantSlice, ok := want.([]any)
		if !ok {
			t.Fatalf("Expected want to be of type []any, got %T", want)
		}
		if len(obj.Items()) != len(wantSlice) {
			t.Fatalf("Expected list length %d, got %d", len(wantSlice), len(obj.Items()))
		}
		for i, item := range obj.Items() {
			AssertObjectEql(t, item, wantSlice[i])
		}
	case *object.Error:
		assert.IsType(t, &object.Error{}, want)
		assert.Equal(t, obj.Payload, want.(*object.Error).Payload)
	default:
		t.Logf("Object: %+v", obj)
		t.Fatalf("Unimplemented object type for assertion: %s", obj.Type())
	}
}
