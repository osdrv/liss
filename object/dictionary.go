package object

import (
	"bytes"
	"encoding/binary"
	"errors"
	"hash/fnv"
	"math"
	"reflect"
)

const (
	DictionaryInitialCap = 32
	MaxLoadFactor        = 0.75
)

var (
	ErrUnhashableType = errors.New("unhashable type")
)

func hashObject(obj Object) (uint64, error) {
	r := obj.Raw()
	h := fnv.New64()
	var b []byte
	switch v := r.(type) {
	case string:
		b = []byte(v)
	case int64:
		b = make([]byte, 8)
		binary.LittleEndian.PutUint64(b, uint64(v))
	case bool:
		if v {
			b = []byte{1}
		} else {
			b = []byte{0}
		}
	case float64:
		b = make([]byte, 8)
		binary.LittleEndian.PutUint64(b, math.Float64bits(v))
	default:
		return 0, ErrUnhashableType
	}
	_, err := h.Write(b)
	return h.Sum64(), err
}

type KeyValue struct {
	Key     Object
	Value   Object
	deleted bool
	next    *KeyValue
}

type Dictionary struct {
	defaultObject
	// TODO: implement gradual resizing
	items []*KeyValue
	len   int
	cnt   int
	cap   int
}

var _ Object = (*Dictionary)(nil)

func NewDictionary() *Dictionary {
	cap := DictionaryInitialCap
	return &Dictionary{
		items: make([]*KeyValue, cap),
		cap:   cap,
		len:   0,
	}
}

func NewDictionaryWithItems(items []Object) (*Dictionary, error) {
	d := NewDictionary()
	for _, item := range items {
		if !item.IsList() || item.(*List).Len() != 2 {
			return nil, errors.New("dictionary items must be lists of key-value pairs")
		}
		kv := item.(*List)
		if err := d.Put(kv.items[0], kv.items[1]); err != nil {
			return nil, err
		}
	}
	return d, nil
}

func equalObjects(a, b Object) bool {
	return a.Type() == b.Type() && reflect.DeepEqual(a.Raw(), b.Raw())
}

func (d *Dictionary) grow() error {
	newcap := d.cap * 2
	newitems := make([]*KeyValue, newcap)
	newcnt := 0
	for _, kv := range d.items {
		ptr := kv
		for ptr != nil {
			if !ptr.deleted {
				h, err := hashObject(ptr.Key)
				if err != nil {
					return err
				}
				h %= uint64(newcap)
				newkv := &KeyValue{
					Key:     ptr.Key,
					Value:   ptr.Value,
					deleted: false,
				}
				newkv.next = newitems[h]
				newitems[h] = newkv
				newcnt++
			}
			ptr = ptr.next
		}
	}
	d.items = newitems
	d.cap = newcap
	d.cnt = newcnt
	return nil
}

func (d *Dictionary) Put(key Object, value Object) error {
	if float64(d.cnt)/float64(d.cap) > MaxLoadFactor {
		if err := d.grow(); err != nil {
			return err
		}
	}

	h, err := hashObject(key)
	if err != nil {
		return err
	}

	h %= uint64(d.cap)
	var kv *KeyValue
	ptr := d.items[h]
	for ptr != nil {
		if equalObjects(ptr.Key, key) {
			kv = ptr
			break
		}
		ptr = ptr.next
	}
	if kv == nil {
		kv = &KeyValue{
			Key:   key,
			Value: value,
		}
		kv.next = d.items[h]
		d.items[h] = kv
	} else {
		kv.Value = value
		kv.deleted = false
	}
	d.len++
	d.cnt++

	return nil
}

func (d *Dictionary) Get(key Object) (Object, bool, error) {
	h, err := hashObject(key)
	if err != nil {
		return nil, false, err
	}

	h %= uint64(d.cap)
	ptr := d.items[h]
	for ptr != nil {
		if equalObjects(ptr.Key, key) {
			if ptr.deleted {
				return nil, false, nil
			}
			return ptr.Value, true, nil
		}
		ptr = ptr.next
	}
	return nil, false, nil
}

func (d *Dictionary) Delete(key Object) (bool, error) {
	h, err := hashObject(key)
	if err != nil {
		return false, err
	}
	h %= uint64(d.cap)
	ptr := d.items[h]
	for ptr != nil {
		if equalObjects(ptr.Key, key) {
			if ptr.deleted {
				return false, nil
			}
			ptr.deleted = true
			d.len--
			return true, nil
		}
		ptr = ptr.next
	}
	return false, nil
}

func (d *Dictionary) Raw() any {
	m := make(map[any]any)
	for _, kv := range d.items {
		ptr := kv
		for ptr != nil {
			if !ptr.deleted {
				m[ptr.Key.Raw()] = ptr.Value.Raw()
			}
			ptr = ptr.next
		}
	}
	return m
}

func (d *Dictionary) Type() ObjectType {
	return DictionaryType
}

// TODO: if I ever come up with a first-class syntax for dictionaries, update this method
func (d *Dictionary) String() string {
	var b bytes.Buffer
	b.WriteString("(dict")

	for _, kv := range d.items {
		ptr := kv
		for ptr != nil {
			if !ptr.deleted {
				b.WriteString(" [")
				b.WriteString(ptr.Key.String())
				b.WriteByte(' ')
				b.WriteString(ptr.Value.String())
				b.WriteByte(']')
			}
			ptr = ptr.next
		}
	}

	b.WriteString(")")
	return b.String()
}

func (d *Dictionary) IsDictionary() bool {
	return true
}

func (d *Dictionary) Len() int {
	return d.len
}

func (d *Dictionary) IsLenable() bool {
	return true
}

func (d *Dictionary) Keys() []Object {
	keys := make([]Object, 0, d.len)
	for _, kv := range d.items {
		ptr := kv
		for ptr != nil {
			if !ptr.deleted {
				keys = append(keys, ptr.Key)
			}
			ptr = ptr.next
		}
	}
	return keys
}

func (d *Dictionary) Values() []Object {
	values := make([]Object, 0, d.len)
	for _, kv := range d.items {
		ptr := kv
		for ptr != nil {
			if !ptr.deleted {
				values = append(values, ptr.Value)
			}
			ptr = ptr.next
		}
	}
	return values
}
