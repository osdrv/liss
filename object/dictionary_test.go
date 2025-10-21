package object

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestDictionaryPutGet(t *testing.T) {
	items := map[int]string{}
	for i := 0; i < 20; i++ {
		items[i] = "value_" + string(rune('A'+i))
	}

	dict := NewDictionary()

	for k, v := range items {
		err := dict.Put(NewInteger(int64(k)), NewString(v))
		assert.NoError(t, err)
	}

	for k, v := range items {
		value, ok, err := dict.Get(NewInteger(int64(k)))
		assert.NoError(t, err)
		assert.True(t, ok)
		strValue, ok := value.(*String)
		assert.True(t, ok)
		assert.Equal(t, v, strValue.Value)
	}
}

func TestDictionaryPutGetDeleteGet(t *testing.T) {
	items := map[int]string{}
	for i := 0; i < 20; i++ {
		items[i] = "value_" + string(rune('A'+i))
	}

	dict := NewDictionary()

	for k, v := range items {
		err := dict.Put(NewInteger(int64(k)), NewString(v))
		assert.NoError(t, err)
	}

	for k := range items {
		ok, err := dict.Delete(NewInteger(int64(k)))
		assert.True(t, ok)
		assert.NoError(t, err)
	}

	for k := range items {
		value, ok, err := dict.Get(NewInteger(int64(k)))
		assert.NoError(t, err)
		assert.False(t, ok)
		assert.Nil(t, value)
	}
}
